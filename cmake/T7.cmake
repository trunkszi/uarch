add_library(uarch_detect STATIC
  "${PROJECT_SOURCE_DIR}/src/detect/cpuid.cc"
  "${PROJECT_SOURCE_DIR}/src/detect/host.cc"
  "${PROJECT_SOURCE_DIR}/src/detect/detect.cc")
add_library(uarch::detect ALIAS uarch_detect)
set_target_properties(uarch_detect PROPERTIES EXPORT_NAME detect)
target_link_libraries(uarch_detect PUBLIC uarch::arch)
uarch_enable_warnings(uarch_detect)
uarch_enable_sanitizers(uarch_detect)
install(TARGETS uarch_detect EXPORT uarch-targets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
