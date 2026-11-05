cmake_minimum_required(VERSION 3.30)
project(uarch VERSION 0.1.0 DESCRIPTION "C++26 microarchitectural primitives" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
option(UARCH_BUILD_TESTS "Build deterministic tests" ${PROJECT_IS_TOP_LEVEL})
option(UARCH_BUILD_BENCHES "Build benchmarks from registered slice fragments" OFF)
option(UARCH_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
include(UarchCompilerOptions)
include(UarchReflection)
include(UarchSanitizers)
include(UarchTargets)
include(UarchCompileFail)
include(UarchCodegenGates)
uarch_detect_reflection()
uarch_create_libraries()

# Slice-owned fragments register hosted libraries or tools without changing this file.
file(GLOB slice_fragments CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/T[0-9]*.cmake")
foreach(fragment IN LISTS slice_fragments)
  include("${fragment}")
endforeach()
if(UARCH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(UARCH_BUILD_BENCHES)
  add_subdirectory(benches)
endif()

install(TARGETS uarch_tiers uarch_core uarch_arch uarch EXPORT uarch-targets)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" OPTIONAL)
install(EXPORT uarch-targets NAMESPACE uarch:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
configure_package_config_file(cmake/uarch-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/uarch-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/uarch-config-version.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/uarch-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/uarch-config-version.cmake"
  cmake/UarchReflection.cmake cmake/UarchCompilerOptions.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uarch")
