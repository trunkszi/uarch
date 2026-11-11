if(NOT UARCH_BUILD_TOOLS)
  return()
endif()

uarch_add_codegen_probes(uarch_probes
  layout_probes.cc
  bits_probes.cc
  flow_probes.cc
  delegate_targets.cc
  arithmetic_probes.cc
  mem_probes.cc
  arch_probes.cc)
