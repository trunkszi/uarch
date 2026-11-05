#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <uarch/probes/gate.hpp>
#include <vector>
namespace uarch::verify {
enum class Status { Pass, Fail, NotRun, Unsupported };
struct Result {
  std::string gate{};
  Status status{Status::Pass};
  std::string detail{};
};
struct Command {
  bool ok{false};
  std::string output{};
};
struct Instruction {
  std::string mnemonic{}, operands{};
};
struct Function {
  std::string symbol{}, section{};
  std::vector<Instruction> instructions{};
  std::string references{};
  std::uint64_t base_addr{0}, max_bytes{0};
};
using Functions = std::map<std::string, Function>;
struct Context {
  std::filesystem::path root;
  std::vector<std::string> objects;
  std::filesystem::path tools;
  std::vector<std::string> mca_cpus{};
  std::vector<std::string> gem5_cpus{};
  std::uint64_t pmu_iterations{200000};
  bool pmu_iterations_explicit{false};
};
std::string read(const std::filesystem::path&);
std::string shell_quote(std::string_view);
Command command(const std::vector<std::string>&, std::string_view input = {});
std::string json_escape(std::string_view);
std::string status_name(Status);
Context default_context();
struct SymbolExtent {
  std::uint64_t address{0}, bytes{0};
};
using SymbolExtents = std::map<std::string, SymbolExtent>;
struct ObjectAssembly {
  bool ok{false};
  Functions functions{};
  std::string error{};
};
Functions parse_assembly(std::string_view, const SymbolExtents& extents = {});
SymbolExtents parse_symbol_extents(std::string_view);
ObjectAssembly inspect_object(const Context&, const std::string&);
std::uint32_t classify(const Instruction&, std::string_view current_symbol = {});
std::vector<std::string> check_function(const probes::Gate&, const Function&);
bool stack_parameter(const Function&);
bool indirect_parameter(const Function&);
bool exception_artifacts(std::string_view);
std::string ir_signature(std::string_view, std::string_view);
bool check_signature(std::string_view, probes::AbiRule);
std::vector<std::string> lint_text(std::string_view path, std::string_view text);
Result lint_gate(const Context&);
Result asm_gate(const Context&);
Result abi_gate(const Context&);
std::vector<std::string> mca_cpu_models(const Context&);
constexpr std::uint64_t combine_probe_checksum(std::uint64_t acc, std::uint64_t index, std::uint64_t value) noexcept {
  return acc * 0x9E3779B97F4A7C15ULL + (value ^ ((index + 1ULL) * 0xBF58476D1CE4E5B9ULL));
}
constexpr bool zen_raw_slots_supported(std::uint16_t family, std::uint16_t model) noexcept {
  return family == 0x1a || (family == 0x19 && ((model >= 0x10 && model <= 0x1f) || (model >= 0x60 && model <= 0x7f)));
}
struct PmuCounters {
  double cycles{0}, instructions{0}, branches{0}, branch_misses{0};
  double stalled_frontend{0}, stalled_backend{0}, retired_ops{0}, dispatched_ops{0};
  double frontend_no_ops_slots{0}, backend_stall_slots{0};
  std::uint64_t slot_width{6};
  double min_active_ratio{1.0};
  bool has_raw_slots{false};
};
struct TmaMetrics {
  double cycles_per_op{0}, instructions_per_op{0}, ipc{0}, branch_misses_per_op{0};
  double retiring_pct{0}, bad_speculation_pct{0}, frontend_bound_pct{0}, backend_bound_pct{0};
  std::string_view dominant_bound{"Retiring"};
};
TmaMetrics compute_tma_metrics(const PmuCounters&, std::uint64_t operations) noexcept;
std::optional<PmuCounters> parse_perf_stat_csv(std::string_view) noexcept;
Result pmu_gate(const Context&);
Result run_pmu_probe(std::string_view name, std::uint64_t iterations);
std::vector<std::string> gem5_cpu_models(const Context&);
struct Gem5Counters {
  double num_cycles{0}, idle_cycles{0}, committed_insts{0}, committed_ops{0};
  double renamed_insts{0}, rename_idle_cycles{0}, rename_squash_cycles{0}, commit_squashed_insts{0};
  double branch_mispredicts{0}, rob_full_events{0}, iq_full_events{0}, lq_full_events{0}, sq_full_events{0};
  double icache_stall_cycles{0}, dcache_misses{0}, mem_order_violations{0};
  bool has_rename_slots{false};
  bool has_structure{false};
};
struct Gem5Metrics {
  double cycles_per_op{0}, insts_per_op{0}, uops_per_op{0}, ipc{0}, branch_mispredicts_per_op{0}, dcache_misses_per_op{0};
  double mem_order_violations{0}, rob_full_per_op{0}, iq_full_per_op{0}, lq_full_per_op{0}, sq_full_per_op{0};
  double retiring_pct{0}, bad_speculation_pct{0}, frontend_bound_pct{0}, backend_bound_pct{0};
  std::string_view dominant_bound{"Retiring"}, attribution_mode{"o3-rename-slot"};
};
std::optional<Gem5Counters> parse_gem5_stats(std::string_view) noexcept;
std::optional<Gem5Counters> diff_gem5_counters(const Gem5Counters&, const Gem5Counters&) noexcept;
std::optional<Gem5Counters> check_gem5_linearity(const Gem5Counters&, std::uint64_t, const Gem5Counters&, std::uint64_t,
                                                 const Gem5Counters&, std::uint64_t, double max_rel_drift = 0.05) noexcept;
std::optional<Gem5Metrics> compute_gem5_metrics(const Gem5Counters&, std::uint64_t, std::uint64_t) noexcept;
std::optional<std::uint64_t> reference_probe_checksum(std::string_view, std::uint64_t) noexcept;
Result gem5_gate(const Context&);
Result mca_gate(const Context&);
Result size_gate(const Context&);
std::vector<Result> execute(const Context&, std::string_view name);
std::string report_json(const Context&, const std::vector<Result>&);
}  // namespace uarch::verify
