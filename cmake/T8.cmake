option(UARCH_BUILD_TOOLS "Build codegen probes and uarch_verify tool" ${UARCH_BUILD_TESTS})
if(NOT UARCH_BUILD_TOOLS)
  return()
endif()

uarch_add_codegen_probes(uarch_probes
  ${PROJECT_SOURCE_DIR}/src/probes/layout_probes.cc
  ${PROJECT_SOURCE_DIR}/src/probes/bits_probes.cc
  ${PROJECT_SOURCE_DIR}/src/probes/flow_probes.cc
  ${PROJECT_SOURCE_DIR}/src/probes/arithmetic_probes.cc
  ${PROJECT_SOURCE_DIR}/src/probes/mem_probes.cc
  ${PROJECT_SOURCE_DIR}/src/probes/arch_probes.cc)

set(UARCH_CLAPP_INCLUDE_DIR "" CACHE PATH "Path to clapp include directory")
set(UARCH_CLAPP_ARCHIVE "$ENV{HOME}/.vcpkg/downloads/clapp-baa7dcd232e6ba0f1152b9813627fe517775a93b.tar.gz"
  CACHE FILEPATH "Local clapp source archive")
add_library(uarch_t8_clapp_headers INTERFACE)
if(UARCH_CLAPP_INCLUDE_DIR)
  if(NOT EXISTS "${UARCH_CLAPP_INCLUDE_DIR}/clapp/clapp.hpp" OR
     NOT EXISTS "${UARCH_CLAPP_INCLUDE_DIR}/clapp/config.hpp")
    message(FATAL_ERROR "UARCH_CLAPP_INCLUDE_DIR must contain configured clapp headers")
  endif()
  target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE "${UARCH_CLAPP_INCLUDE_DIR}")
else()
  find_package(clapp CONFIG QUIET)
  if(TARGET clapp::clapp)
    # clapp is header-only; root already validates reflection for the active compiler.
    # Reuse headers without importing a different compiler's reflection flag spelling.
    get_target_property(t8_clapp_includes clapp::clapp INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE ${t8_clapp_includes})
  elseif(EXISTS "$ENV{HOME}/.vcpkg/packages/clapp_x64-linux-musl-clang-release/include/clapp/config.hpp")
    target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE
      "$ENV{HOME}/.vcpkg/packages/clapp_x64-linux-musl-clang-release/include")
  elseif(EXISTS "$ENV{HOME}/.local/vcpkg/clapp/include/clapp/clapp.hpp" AND
         EXISTS "$ENV{HOME}/.local/vcpkg/clapp/include/clapp/config.hpp")
    target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE "$ENV{HOME}/.local/vcpkg/clapp/include")
  elseif(EXISTS "/root/.local/vcpkg/clapp/include/clapp/clapp.hpp" AND
         EXISTS "/root/.local/vcpkg/clapp/include/clapp/config.hpp")
    target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE "/root/.local/vcpkg/clapp/include")
  else()
    include(FetchContent)
    # Local source archives keep configuration independent of network access.
    if(EXISTS "${UARCH_CLAPP_ARCHIVE}")
      FetchContent_Declare(uarch_t8_clapp URL "${UARCH_CLAPP_ARCHIVE}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR uarch-header-only-integration)
    else()
      message(FATAL_ERROR "Local clapp archive or headers not found; supported inputs: UARCH_CLAPP_INCLUDE_DIR, UARCH_CLAPP_ARCHIVE, ~/.vcpkg/packages, ~/.local/vcpkg/clapp")
    endif()
    FetchContent_MakeAvailable(uarch_t8_clapp)
    set(CLAPP_META_HEADER_EXPERIMENTAL 0)
    set(CLAPP_HAS_REFLECTION 1)
    set(CLAPP_HAS_ANNOTATIONS 1)
    set(CLAPP_HAS_EXPANSION_STATEMENTS 1)
    configure_file(${uarch_t8_clapp_SOURCE_DIR}/cmake/config.hpp.in
      ${PROJECT_BINARY_DIR}/t8-generated/clapp/config.hpp @ONLY)
    target_include_directories(uarch_t8_clapp_headers SYSTEM INTERFACE
      ${uarch_t8_clapp_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/t8-generated)
  endif()
endif()

set(t8_verify_sources
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/lint_gate.cc
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/asm_gate.cc
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/abi_gate.cc
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/mca_size_gate.cc
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/pmu_gate.cc
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/gem5_gate.cc)
set(UARCH_LLVM_TOOLS_DIR "/root/.local/toolchain/clang-glibc/bin" CACHE PATH "Directory containing LLVM binary utilities")
uarch_compiler_options(t8_warning_flags)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(t8_reflection_flags -freflection-latest -stdlib=libc++)
else()
  set(t8_reflection_flags -freflection)
endif()
string(JOIN " " t8_flags -std=c++26 -O3 -fno-lto -fno-exceptions -fno-rtti ${t8_reflection_flags} ${t8_warning_flags})
add_executable(uarch_gem5_probe ${PROJECT_SOURCE_DIR}/src/tools/uarch_gem5_probe/main.cc $<TARGET_OBJECTS:uarch_probes>)
uarch_configure_internal(uarch_gem5_probe NO_SANITIZERS)
target_link_libraries(uarch_gem5_probe PRIVATE uarch::arch)
target_compile_options(uarch_gem5_probe PRIVATE -O3 -fno-lto -fno-exceptions -fno-rtti -fno-stack-protector -fno-builtin -fno-pie)
target_link_options(uarch_gem5_probe PRIVATE -fno-lto -nostdlib -static -Wl,-no-pie,--no-dynamic-linker)
set_target_properties(uarch_gem5_probe PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(uarch_gem5_probe PRIVATE -fno-profile-instr-generate -fno-coverage-mapping)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(uarch_gem5_probe PRIVATE -fno-profile-arcs -fno-test-coverage -fno-profile-generate)
endif()
file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/t8-generated/uarch_verify_config.hpp CONTENT
"#pragma once
#include <string_view>
namespace uarch::verify::config {
inline constexpr std::string_view gem5_probe = R\"cfg($<TARGET_FILE:uarch_gem5_probe>)cfg\";
inline constexpr std::string_view root = R\"cfg(${PROJECT_SOURCE_DIR})cfg\";
inline constexpr std::string_view objects = R\"cfg($<JOIN:$<TARGET_OBJECTS:uarch_probes>,\n>)cfg\";
inline constexpr std::string_view tools = R\"cfg(${UARCH_LLVM_TOOLS_DIR})cfg\";
inline constexpr std::string_view compiler = R\"cfg(${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})cfg\";
inline constexpr std::string_view flags = R\"cfg(${t8_flags})cfg\";
}")
add_library(uarch_verify_lib STATIC ${t8_verify_sources} $<TARGET_OBJECTS:uarch_probes>)
find_package(Threads REQUIRED)
target_link_libraries(uarch_verify_lib PUBLIC uarch::detect Threads::Threads)
uarch_configure_internal(uarch_verify_lib)
target_include_directories(uarch_verify_lib PUBLIC
  ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify ${PROJECT_BINARY_DIR}/t8-generated)
add_dependencies(uarch_verify_lib uarch_probes uarch_gem5_probe)
add_executable(uarch_verify ${PROJECT_SOURCE_DIR}/src/tools/uarch_verify/main.cc)
uarch_configure_internal(uarch_verify)
target_link_libraries(uarch_verify PRIVATE uarch_verify_lib uarch_t8_clapp_headers)
