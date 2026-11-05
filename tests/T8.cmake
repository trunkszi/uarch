if(NOT TARGET uarch_verify)
  return()
endif()

uarch_add_test(uarch_verify_test unit/tools/uarch_verify_test.cc)
target_link_libraries(uarch_verify_test PRIVATE uarch_verify_lib)
add_dependencies(uarch_verify_test uarch_verify)
add_test(NAME uarch_verify_all COMMAND uarch_verify all)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(t8_default_foreign_compiler /root/.local/toolchain/gcc-glibc/bin/x86_64-unknown-linux-gnu-g++)
  set(t8_foreign_flags -freflection)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(t8_default_foreign_compiler /root/.local/toolchain/clang-glibc/bin/clang++)
  set(t8_foreign_flags -freflection-latest -stdlib=libc++)
else()
  message(FATAL_ERROR "T8 cross ABI requires Clang or GCC")
endif()
set(UARCH_FOREIGN_CXX_COMPILER "${t8_default_foreign_compiler}" CACHE FILEPATH "Opposite compiler for the cross-compiler ABI test")
if(NOT EXISTS "${UARCH_FOREIGN_CXX_COMPILER}")
  message(STATUS "NotRun: missing opposite compiler ${UARCH_FOREIGN_CXX_COMPILER}; skipping abi_cross_test")
  return()
endif()
file(GLOB_RECURSE t8_abi_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hpp)
set(t8_foreign_object ${CMAKE_CURRENT_BINARY_DIR}/abi_callee_foreign.o)
add_custom_command(OUTPUT ${t8_foreign_object}
  COMMAND ${UARCH_FOREIGN_CXX_COMPILER} -std=c++26 ${t8_foreign_flags}
    -fno-exceptions -fno-rtti -O3 -fno-lto -I${PROJECT_SOURCE_DIR}/include
    -c ${CMAKE_CURRENT_SOURCE_DIR}/abi_cross/abi_callee.cc -o ${t8_foreign_object}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/abi_cross/abi_callee.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/abi_cross/abi_types.hpp ${t8_abi_headers}
  VERBATIM)
set_source_files_properties(${t8_foreign_object} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
uarch_add_test(abi_cross_test abi_cross/abi_caller_test.cc ${t8_foreign_object})
