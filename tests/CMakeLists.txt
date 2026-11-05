uarch_add_test(smoke_test unit/smoke_test.cc)
uarch_add_compile_fail(smoke_compile_fail compile_fail/smoke_compile_fail.cc)

# Each slice owns tests/T<N>.cmake and calls uarch_add_test/uarch_add_compile_fail there.
# CONFIGURE_DEPENDS discovers new fragments without edits to this shared entry point.
file(GLOB slice_fragments CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/T[0-9]*.cmake")
foreach(fragment IN LISTS slice_fragments)
  include("${fragment}")
endforeach()

# A source caret snippet must not satisfy the diagnostic matcher.
uarch_add_compile_fail(caret_diagnostic_control compile_fail/smoke_compile_fail.cc EXPECT_DIAGNOSTIC_REJECTION)
target_compile_definitions(caret_diagnostic_control PRIVATE UARCH_CARET_FAILURE_CONTROL=1)

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
target_link_libraries(mixed_c_consumer PRIVATE uarch::uarch)
target_compile_options(mixed_c_consumer PRIVATE -Wall -Wextra -Werror)
add_test(NAME mixed_c_consumer COMMAND mixed_c_consumer)
