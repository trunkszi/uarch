include_guard(GLOBAL)

function(uarch_create_libraries)
  foreach(layer IN ITEMS tiers core arch)
    add_library(uarch_${layer} INTERFACE)
    add_library(uarch::${layer} ALIAS uarch_${layer})
    set_target_properties(uarch_${layer} PROPERTIES EXPORT_NAME ${layer})
  endforeach()
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
  target_link_libraries(uarch INTERFACE uarch::arch)
endfunction()

function(uarch_configure_internal target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "NO_SANITIZERS" "" "")
  target_link_libraries(${target} PRIVATE uarch::uarch)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}")
    # A sysroot libc in RUNPATH must use its matching loader, not the host glibc loader.
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=ld-linux-x86-64.so.2
      OUTPUT_VARIABLE runtime_loader OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE loader_status)
    if(loader_status EQUAL 0 AND IS_ABSOLUTE "${runtime_loader}" AND EXISTS "${runtime_loader}")
      target_link_options(${target} PRIVATE "LINKER:--dynamic-linker,${runtime_loader}")
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(directory IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
      # Use compiler runtimes without replacing the host loader's libc.
      if(NOT directory MATCHES "/sysroot(/|$)")
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${directory}")
      endif()
    endforeach()
  endif()
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
