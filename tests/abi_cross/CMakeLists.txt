if(NOT TARGET uarch_verify)
  return()
endif()

uarch_resolve_opposite_compiler(_uarch_foreign_compiler _uarch_foreign_flags)
set(UARCH_FOREIGN_CXX_COMPILER "${_uarch_foreign_compiler}" CACHE FILEPATH
  "Opposite compiler for the cross-compiler ABI test")
if(NOT EXISTS "${UARCH_FOREIGN_CXX_COMPILER}")
  message(STATUS "NotRun: missing opposite compiler ${UARCH_FOREIGN_CXX_COMPILER}; skipping abi_cross_test")
  return()
endif()
file(GLOB_RECURSE abi_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hpp)
set(foreign_object ${CMAKE_CURRENT_BINARY_DIR}/abi_callee_foreign.o)
add_custom_command(OUTPUT ${foreign_object}
  COMMAND ${UARCH_FOREIGN_CXX_COMPILER} -std=c++26 ${_uarch_foreign_flags}
    -fno-exceptions -fno-rtti -O3 -fno-lto -I${PROJECT_SOURCE_DIR}/include
    -c ${CMAKE_CURRENT_SOURCE_DIR}/abi_callee.cc -o ${foreign_object}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/abi_callee.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/abi_types.hpp ${abi_headers}
  VERBATIM)
set_source_files_properties(${foreign_object} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
uarch_add_test(abi_cross_test abi_caller_test.cc ${foreign_object})
