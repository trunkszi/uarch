include_guard(GLOBAL)

function(uarch_compiler_options output)
  set(options -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Wconversion
    -Wsign-conversion -Wswitch -Werror -ffunction-sections -fdata-sections)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC diagnoses named non-template expansion variables as shadowing their own generated declarations.
    list(APPEND options -Wno-shadow)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND options -Wshadow)
  else()
    message(FATAL_ERROR "uarch requires a verified GCC or Clang C++26 compiler")
  endif()
  set(${output} "${options}" PARENT_SCOPE)
endfunction()

function(uarch_enable_warnings target)
  uarch_compiler_options(options)
  target_compile_options(${target} PRIVATE ${options})
endfunction()
