cmake_minimum_required(VERSION 4.0)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UarchToolchain.cmake")
uarch_ensure_toolchain()

project(uarch VERSION 0.1.0 DESCRIPTION "C++26 microarchitectural primitives" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
option(UARCH_BUILD_TESTS "Build deterministic tests" ${PROJECT_IS_TOP_LEVEL})
option(UARCH_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(UARCH_BUILD_BENCHES "Build benchmarks" OFF)
option(UARCH_BUILD_TOOLS "Build codegen probes and uarch_verify" ${UARCH_BUILD_TESTS})
option(UARCH_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(UARCH_INSTALL_PACKAGE "Install the uarch CMake package" ON)
include(UarchCompilerOptions)
include(UarchReflection)
include(UarchSanitizers)
include(UarchTargets)
include(UarchCompileFail)
include(UarchCodegenGates)
uarch_detect_reflection()
uarch_create_libraries()

add_subdirectory(src)
target_link_libraries(uarch INTERFACE uarch::detect)
if(UARCH_BUILD_TESTS)
  enable_testing()
endif()
if(UARCH_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(UARCH_BUILD_TESTS)
  add_subdirectory(tests)
endif()
if(UARCH_BUILD_BENCHES)
  add_subdirectory(benches)
endif()

if(UARCH_INSTALL_PACKAGE)
  install(TARGETS uarch_tiers uarch_core uarch_arch uarch_primitives uarch EXPORT uarch-targets)
  install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(EXPORT uarch-targets NAMESPACE uarch:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
  configure_package_config_file(cmake/uarch-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/uarch-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/uarch-config-version.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/uarch-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/uarch-config-version.cmake"
    cmake/UarchToolchain.cmake cmake/UarchReflection.cmake cmake/UarchCompilerOptions.cmake
    cmake/UarchRuntime.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
endif()
