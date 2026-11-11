add_subdirectory(unit)
add_subdirectory(consteval)
add_subdirectory(compile_fail)
add_subdirectory(abi_cross)

if(UARCH_ENABLE_SANITIZERS)
  uarch_add_test(ubsan_failure_control unit/smoke_test.cc)
  target_compile_definitions(ubsan_failure_control PRIVATE UARCH_UBSAN_FAILURE_CONTROL=1)
  set_tests_properties(ubsan_failure_control PROPERTIES WILL_FAIL TRUE)
endif()

# This object must stay free of sanitizer instrumentation even in sanitizer presets.
uarch_add_codegen_probes(native_smoke_probe unit/smoke_test.cc)

enable_language(C)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mixed_consumer.c" "int main(void) { return 0; }\n")
add_executable(mixed_c_consumer "${CMAKE_CURRENT_BINARY_DIR}/mixed_consumer.c")
# The hosted facade also links the C++ detection archive, which a C linker cannot pull in.
target_link_libraries(mixed_c_consumer PRIVATE uarch::primitives)
uarch_configure_runtime(mixed_c_consumer)
target_compile_options(mixed_c_consumer PRIVATE -Wall -Wextra -Werror)
add_test(NAME mixed_c_consumer COMMAND mixed_c_consumer)
