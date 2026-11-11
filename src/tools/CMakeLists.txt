if(NOT UARCH_BUILD_TOOLS)
  return()
endif()

set(UARCH_CLAPP_INCLUDE_DIR "" CACHE PATH "Path to configured clapp headers")
add_library(uarch_clapp_headers INTERFACE)
if(UARCH_CLAPP_INCLUDE_DIR)
  if(NOT EXISTS "${UARCH_CLAPP_INCLUDE_DIR}/clapp/clapp.hpp" OR
     NOT EXISTS "${UARCH_CLAPP_INCLUDE_DIR}/clapp/config.hpp")
    message(FATAL_ERROR "UARCH_CLAPP_INCLUDE_DIR must contain configured clapp headers")
  endif()
  target_include_directories(uarch_clapp_headers SYSTEM INTERFACE "${UARCH_CLAPP_INCLUDE_DIR}")
else()
  find_package(clapp CONFIG REQUIRED)
  # The active compiler's reflection flags can differ from those exported by the header package.
  get_target_property(clapp_includes clapp::clapp INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(uarch_clapp_headers SYSTEM INTERFACE ${clapp_includes})
endif()

set(verify_sources
  uarch_verify/lint_gate.cc
  uarch_verify/asm_gate.cc
  uarch_verify/abi_gate.cc
  uarch_verify/mca_size_gate.cc
  uarch_verify/pmu_gate.cc
  uarch_verify/gem5_gate.cc)
uarch_resolve_llvm_tools_dir(_uarch_default_llvm_tools)
if(NOT DEFINED CACHE{UARCH_LLVM_TOOLS_DIR} OR "$CACHE{UARCH_LLVM_TOOLS_DIR}" MATCHES "/clang-musl/bin$")
  set(UARCH_LLVM_TOOLS_DIR "${_uarch_default_llvm_tools}" CACHE PATH
    "Directory containing LLVM binary utilities" FORCE)
endif()
uarch_compiler_options(warning_flags)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(reflection_flags -freflection-latest -stdlib=libc++)
else()
  set(reflection_flags -freflection)
endif()
string(JOIN " " verify_flags -std=c++26 -O3 -fno-lto -fno-exceptions -fno-rtti ${reflection_flags} ${warning_flags})
add_executable(uarch_gem5_probe uarch_gem5_probe/main.cc $<TARGET_OBJECTS:uarch_probes>)
uarch_configure_internal(uarch_gem5_probe NO_SANITIZERS PRIMITIVES_ONLY)
target_compile_options(uarch_gem5_probe PRIVATE -O3 -fno-lto -fno-exceptions -fno-rtti -fno-stack-protector -fno-builtin -fno-pie)
target_link_options(uarch_gem5_probe PRIVATE -fno-lto -nostdlib -static -Wl,-no-pie,--no-dynamic-linker)
set_target_properties(uarch_gem5_probe PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # The freestanding link deliberately ignores the toolchain's hosted runtime selectors.
  target_link_options(uarch_gem5_probe PRIVATE -Wno-unused-command-line-argument)
  target_compile_options(uarch_gem5_probe PRIVATE -fno-profile-instr-generate -fno-coverage-mapping)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(uarch_gem5_probe PRIVATE -fno-profile-arcs -fno-test-coverage -fno-profile-generate)
endif()
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/uarch_verify_config.hpp CONTENT
"#pragma once
#include <string_view>
namespace uarch::verify::config {
inline constexpr std::string_view gem5_probe = R\"cfg($<TARGET_FILE:uarch_gem5_probe>)cfg\";
inline constexpr std::string_view root = R\"cfg(${PROJECT_SOURCE_DIR})cfg\";
inline constexpr std::string_view objects = R\"cfg($<JOIN:$<TARGET_OBJECTS:uarch_probes>,\n>)cfg\";
inline constexpr std::string_view tools = R\"cfg(${UARCH_LLVM_TOOLS_DIR})cfg\";
inline constexpr std::string_view compiler = R\"cfg(${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})cfg\";
inline constexpr std::string_view flags = R\"cfg(${verify_flags})cfg\";
}")
add_library(uarch_verify_lib STATIC ${verify_sources} $<TARGET_OBJECTS:uarch_probes>)
find_package(Threads REQUIRED)
target_link_libraries(uarch_verify_lib PUBLIC uarch::detect Threads::Threads)
uarch_configure_internal(uarch_verify_lib)
target_include_directories(uarch_verify_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/uarch_verify ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_dependencies(uarch_verify_lib uarch_probes uarch_gem5_probe)
add_executable(uarch_verify uarch_verify/main.cc)
uarch_configure_internal(uarch_verify)
target_link_libraries(uarch_verify PRIVATE uarch_verify_lib uarch_clapp_headers)
