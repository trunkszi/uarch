include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/UarchRuntime.cmake")

function(uarch_create_libraries)
  foreach(layer IN ITEMS tiers core arch)
    add_library(uarch_${layer} INTERFACE)
    add_library(uarch::${layer} ALIAS uarch_${layer})
    set_target_properties(uarch_${layer} PROPERTIES EXPORT_NAME ${layer})
  endforeach()
  add_library(uarch_primitives INTERFACE)
  add_library(uarch::primitives ALIAS uarch_primitives)
  set_target_properties(uarch_primitives PROPERTIES EXPORT_NAME primitives)
  add_library(uarch::freestanding ALIAS uarch_primitives)
  add_library(uarch INTERFACE)
  add_library(uarch::uarch ALIAS uarch)
  target_compile_features(uarch_tiers INTERFACE cxx_std_26)
  target_include_directories(uarch_tiers INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
  target_compile_options(uarch_tiers INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:-freflection>"
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-freflection-latest>"
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-stdlib=libc++>")
  target_link_options(uarch_tiers INTERFACE
    "$<$<LINK_LANG_AND_ID:CXX,GNU>:-freflection>"
    "$<$<LINK_LANG_AND_ID:CXX,Clang>:-freflection-latest>"
    "$<$<LINK_LANG_AND_ID:CXX,Clang>:-stdlib=libc++>")
  target_link_libraries(uarch_core INTERFACE uarch::tiers)
  target_link_libraries(uarch_arch INTERFACE uarch::core)
  target_link_libraries(uarch_primitives INTERFACE uarch::arch)
  target_link_libraries(uarch INTERFACE uarch::primitives)
endfunction()

function(uarch_configure_internal target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NO_SANITIZERS;PRIMITIVES_ONLY" "" "")
  if(ARG_PRIMITIVES_ONLY)
    target_link_libraries(${target} PRIVATE uarch::primitives)
  else()
    target_link_libraries(${target} PRIVATE uarch::uarch)
  endif()
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  uarch_configure_runtime(${target})
  uarch_enable_warnings(${target})
  if(NOT ARG_NO_SANITIZERS)
    uarch_enable_sanitizers(${target})
  endif()
endfunction()

function(uarch_add_test target)
  add_executable(${target} ${ARGN})
  uarch_configure_internal(${target})
  add_test(NAME ${target} COMMAND ${target})
endfunction()
