add_executable(detect_bench detect/detect_bench.cc)
uarch_configure_internal(detect_bench)
target_link_libraries(detect_bench PRIVATE uarch::detect)
