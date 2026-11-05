#include <charconv>
#include <clapp/clapp.hpp>
#include <cstdio>
#include <string>

#include "verify.hpp"

struct[[= clapp::cmd{.name = "uarch_verify", .version = "0.1.0"}]] Options {
  [[= clapp::arg{.index = 1,
                 .help = "lint, asm-check, abi-check, mca-gate, size-report, pmu-gate, gem5-gate, all",
                 .default_value = "all"}]] std::string gate;
  [[= clapp::arg{.long_ = "gem5-cpu", .help = "Parametric O3 profiles", .default_value = ""}]] std::optional<std::string>
      gem5_cpu;
  [[= clapp::arg{
      .long_ = "mca-cpu", .help = "Comma-separated LLVM CPU models", .default_value = ""}]] std::optional<std::string>
      mca_cpu;
  [[= clapp::arg{
      .long_ = "pmu-iterations", .help = "Operations per PMU probe", .default_value = ""}]] std::optional<std::string>
      pmu_iterations;
  [[= clapp::arg{.long_ = "pmu-probe", .help = "Internal perf runner probe", .default_value = ""}]] std::optional<std::string>
      pmu_probe;
};
int main(int argc, char** argv) {
  const auto options = clapp::parse<Options>(argc, argv);
  auto context = uarch::verify::default_context();
  if (options.gem5_cpu) context.gem5_cpus = {*options.gem5_cpu};
  if (options.mca_cpu) context.mca_cpus = {*options.mca_cpu};
  if (options.pmu_iterations) {
    const auto& text = *options.pmu_iterations;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), context.pmu_iterations);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !context.pmu_iterations) {
      std::fputs("pmu-iterations must be a positive integer\n", stderr);
      return 1;
    }
    context.pmu_iterations_explicit = true;
  }
  const auto results = options.gate == "__pmu-probe" ? std::vector<uarch::verify::Result>{uarch::verify::run_pmu_probe(
                                                           options.pmu_probe.value_or(""), context.pmu_iterations)}
                                                     : uarch::verify::execute(context, options.gate);
  const auto report = uarch::verify::report_json(context, results);
  std::fwrite(report.data(), 1, report.size(), stdout);
  for (const auto& result : results)
    if (result.status == uarch::verify::Status::Fail || result.status == uarch::verify::Status::Unsupported) return 1;
  return 0;
}
