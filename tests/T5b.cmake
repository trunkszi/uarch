uarch_add_test(vec_test unit/core/simd/vec_test.cc)
uarch_add_test(simd_consteval_test consteval/core/simd/simd_consteval_test.cc)
uarch_add_compile_fail(invalid_simd_lanes_fail compile_fail/core/simd/invalid_simd_lanes_fail.cc)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  target_compile_options(vec_test PRIVATE -mprefer-vector-width=512)
endif()
uarch_add_test(inplace_vector_test unit/core/container/inplace_vector_test.cc)
uarch_add_compile_fail(inplace_vector_push_back_fail compile_fail/core/container/inplace_vector_push_back_fail.cc)
