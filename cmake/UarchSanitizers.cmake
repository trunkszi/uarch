include_guard(GLOBAL)

function(uarch_enable_sanitizers target)
  if(UARCH_ENABLE_SANITIZERS)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
  endif()
endfunction()
