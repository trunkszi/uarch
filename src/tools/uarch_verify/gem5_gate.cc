#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include <sstream>
#include <thread>
#include <uarch_verify_config.hpp>

#include "verify.hpp"
namespace uarch::verify {
namespace {
constexpr std::array profiles{"o3-w6-rob320", "o3-w8-rob448", "o3-w6-rob512", "o3-a64-w6-rob320"};
constexpr std::array probes{"bitword_low_matches",      "bitword_high_matches",  "flow_select_register_u64",
                            "flow_select_u64",          "fast_divisor_u64_div",  "fast_divisor_u32_rem",
                            "uarch_arch_scalar_gather", "bit_ones_sum",          "x86_v3_gather",
                            "x86_v3_slow_pdep_gather",  "uarch_arch_masked_sum", "uarch_arch_first_ge"};
constexpr std::array<double Gem5Counters::*, 16> counters{
    &Gem5Counters::num_cycles,           &Gem5Counters::idle_cycles,           &Gem5Counters::committed_insts,
    &Gem5Counters::committed_ops,        &Gem5Counters::renamed_insts,         &Gem5Counters::rename_idle_cycles,
    &Gem5Counters::rename_squash_cycles, &Gem5Counters::commit_squashed_insts, &Gem5Counters::branch_mispredicts,
    &Gem5Counters::rob_full_events,      &Gem5Counters::iq_full_events,        &Gem5Counters::lq_full_events,
    &Gem5Counters::sq_full_events,       &Gem5Counters::icache_stall_cycles,   &Gem5Counters::dcache_misses,
    &Gem5Counters::mem_order_violations};
constexpr std::array<std::array<std::string_view, 4>, 16> aliases{
    {{"numCycles"},
     {"idleCycles"},
     {"commitStats0.numInsts", "commit.instsCommitted::total", "commit.instsCommitted", "committedInsts"},
     {"commitStats0.numOps", "commit.opsCommitted::total", "commit.opsCommitted", "committedOps"},
     {"rename.renamedInsts", "rename.RenamedInsts"},
     {"rename.idleCycles", "rename.IdleCycles", "rename.status::Idle"},
     {"rename.squashCycles", "rename.SquashCycles", "rename.status::Squashing"},
     {"commit.commitSquashedInsts", "commit.squashedInsts"},
     {"commit.branchMispredicts", "branchPred.condIncorrect"},
     {"rename.ROBFullEvents"},
     {"rename.IQFullEvents"},
     {"rename.LQFullEvents"},
     {"rename.SQFullEvents"},
     {"fetchStats0.icacheStallCycles", "fetch.icacheStallCycles"},
     {"dcache.overallMisses::total", "dcache.overall_misses::total"},
     {"iew.memOrderViolationEvents"}}};
std::string_view trim(std::string_view s) {
  auto start = s.find_first_not_of(" \t\r\n");
  return start == std::string_view::npos ? std::string_view{} : s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
}
std::optional<double> number(std::string_view s) {
  double v = 0;
  auto p = std::from_chars(s.data(), s.data() + s.size(), v);
  if (s.empty() || p.ec != std::errc{} || p.ptr != s.data() + s.size() || !std::isfinite(v) || v < 0) return std::nullopt;
  return v;
}
bool valid(const Gem5Counters& c) {
  return std::ranges::all_of(counters, [&](auto member) { return std::isfinite(c.*member) && c.*member >= 0; });
}
std::optional<std::uint64_t> integer(std::string_view s) {
  int base = 10;
  if (s.starts_with("0x") || s.starts_with("0X")) {
    s.remove_prefix(2);
    base = 16;
  }
  std::uint64_t n = 0;
  auto p = std::from_chars(s.data(), s.data() + s.size(), n, base);
  if (s.empty() || p.ec != std::errc{} || p.ptr != s.data() + s.size()) return std::nullopt;
  return n;
}
bool guest_output(std::string_view text, std::string_view probe, std::uint64_t n) {
  const auto expected = reference_probe_checksum(probe, n);
  if (!expected) return false;
  std::istringstream lines{std::string(text)};
  std::string line;
  unsigned seen = 0;
  while (std::getline(lines, line)) {
    if (!line.starts_with("UARCH_GEM5")) continue;
    ++seen;
    std::istringstream words(line);
    std::string tag, p, count, sum, extra;
    words >> tag >> p >> count >> sum;
    if (words >> extra || tag != "UARCH_GEM5" || p != "probe=" + std::string(probe) || !count.starts_with("n=") ||
        !sum.starts_with("checksum="))
      return false;
    if (integer(std::string_view(count).substr(2)) != n || integer(std::string_view(sum).substr(9)) != expected) return false;
  }
  return seen == 1;
}
}  // namespace
std::vector<std::string> gem5_cpu_models(const Context& context) {
  auto sources = context.gem5_cpus;
  if (sources.empty()) {
    if (const auto* env = std::getenv("UARCH_GEM5_CPU"))
      sources.emplace_back(env);
    else {
#if defined(__aarch64__)
      sources = {profiles[3]};
#else
      sources = {profiles[0], profiles[1], profiles[2]};
#endif
    }
  }
  std::vector<std::string> result;
  for (const auto& source : sources) {
    auto raw = trim(source);
    if (raw.empty() || raw.front() == ',' || raw.back() == ',') return {};
    while (!raw.empty()) {
      auto end = raw.find(',');
      auto name = std::string(trim(raw.substr(0, end)));
      raw = end == std::string_view::npos ? std::string_view{} : raw.substr(end + 1);
      if (name == "zen4-o3" || name == "znver4")
        name = profiles[0];
      else if (name == "zen5-o3" || name == "znver5")
        name = profiles[1];
      else if (name == "sapphirerapids-o3" || name == "sapphirerapids")
        name = profiles[2];
      else if (name == "neoverse-v2-o3" || name == "neoverse-v2")
        name = profiles[3];
      if (std::ranges::find(profiles, name) == profiles.end()) return {};
      if (std::ranges::find(result, name) == result.end()) result.push_back(name);
    }
  }
  return result;
}
std::optional<Gem5Counters> parse_gem5_stats(std::string_view text) noexcept {
  Gem5Counters result;
  std::array<bool, 16> seen{};
  std::map<std::string, std::string> raw;
  std::string prefix;
  unsigned begins = 0, ends = 0;
  std::istringstream lines{std::string(text)};
  std::string line;
  while (std::getline(lines, line)) {
    if (line.find("Begin Simulation Statistics") != std::string_view::npos) ++begins;
    if (line.find("End Simulation Statistics") != std::string_view::npos) ++ends;
    std::istringstream words(line);
    std::string key, value;
    words >> key >> value;
    constexpr std::array<std::string_view, 3> cpu_prefixes{"system.cpu.", "board.processor.cores.core.",
                                                           "board.processor.cores0.core."};
    const auto found = std::ranges::find_if(cpu_prefixes, [&](auto candidate) { return key.starts_with(candidate); });
    if (key.ends_with(".numCycles") && (found == cpu_prefixes.end() || key != std::string(*found) + "numCycles"))
      return std::nullopt;
    if (found == cpu_prefixes.end()) continue;
    if (!prefix.empty() && prefix != *found) return std::nullopt;
    prefix = *found;
    const auto stat = key.substr(found->size());
    if (!raw.emplace(stat, value).second) return std::nullopt;
  }
  if (begins > 1 || begins != ends) return std::nullopt;
  for (std::size_t index = 0; index < aliases.size(); ++index) {
    for (auto alias : aliases[index]) {
      if (alias.empty()) continue;
      const auto found = raw.find(std::string(alias));
      if (found == raw.end()) continue;
      const auto count = number(found->second);
      if (!count) return std::nullopt;
      seen[index] = true;
      result.*counters[index] = *count;
      break;
    }
  }
  if (!seen[0] || !seen[2] || !seen[3]) return std::nullopt;
  result.has_rename_slots = seen[4] && seen[5] && seen[6];
  result.has_structure = seen[8] && seen[15];
  return result;
}
std::optional<Gem5Counters> diff_gem5_counters(const Gem5Counters& high, const Gem5Counters& low) noexcept {
  if (!valid(high) || !valid(low) || high.has_rename_slots != low.has_rename_slots) return std::nullopt;
  Gem5Counters delta;
  delta.has_rename_slots = high.has_rename_slots;
  delta.has_structure = high.has_structure && low.has_structure;
  for (auto member : counters) {
    if (member == &Gem5Counters::commit_squashed_insts) {
      delta.*member = std::max(0.0, high.*member - low.*member);
      continue;
    }
    if (high.*member < low.*member) return std::nullopt;
    delta.*member = high.*member - low.*member;
  }
  return delta;
}
std::optional<Gem5Counters> check_gem5_linearity(const Gem5Counters& r0, std::uint64_t n0, const Gem5Counters& r1,
                                                 std::uint64_t n1, const Gem5Counters& r2, std::uint64_t n2,
                                                 double drift) noexcept {
  if (!(n0 < n1 && n1 < n2) || !std::isfinite(drift) || drift < 0) return std::nullopt;
  const auto a = diff_gem5_counters(r1, r0), b = diff_gem5_counters(r2, r1);
  if (!a || !b || a->num_cycles <= 0 || b->num_cycles <= 0 || a->committed_ops <= 0 || b->committed_ops <= 0)
    return std::nullopt;
  const auto s1 = a->num_cycles / static_cast<double>(n1 - n0), s2 = b->num_cycles / static_cast<double>(n2 - n1);
  if (std::abs(s2 - s1) / s1 > drift) return std::nullopt;
  auto result = *b;
  result.has_structure = a->has_structure && b->has_structure;
  return result;
}
std::optional<Gem5Metrics> compute_gem5_metrics(const Gem5Counters& c, std::uint64_t operations, std::uint64_t width) noexcept {
  if (!valid(c) || !operations || !width || !c.has_rename_slots || c.num_cycles <= 0 || c.idle_cycles > c.num_cycles)
    return std::nullopt;
  const auto ops = static_cast<double>(operations), w = static_cast<double>(width);
  const auto slots = w * std::max(1.0, c.num_cycles - c.idle_cycles);
  if (!std::isfinite(slots)) return std::nullopt;
  Gem5Metrics m;
  m.cycles_per_op = c.num_cycles / ops;
  m.insts_per_op = c.committed_insts / ops;
  m.uops_per_op = c.committed_ops / ops;
  m.ipc = c.committed_insts / c.num_cycles;
  m.branch_mispredicts_per_op = c.branch_mispredicts / ops;
  m.dcache_misses_per_op = c.dcache_misses / ops;
  m.mem_order_violations = c.mem_order_violations;
  m.rob_full_per_op = c.rob_full_events / ops;
  m.iq_full_per_op = c.iq_full_events / ops;
  m.lq_full_per_op = c.lq_full_events / ops;
  m.sq_full_per_op = c.sq_full_events / ops;
  m.retiring_pct = 100 * c.committed_ops / slots;
  m.bad_speculation_pct = 100 * (std::max(0.0, c.renamed_insts - c.committed_ops) + w * c.rename_squash_cycles) / slots;
  m.frontend_bound_pct = 100 * w * c.rename_idle_cycles / slots;
  const auto total = m.retiring_pct + m.bad_speculation_pct + m.frontend_bound_pct;
  if (!std::isfinite(total) || total > 105) return std::nullopt;
  m.backend_bound_pct = std::clamp(100 - total, 0.0, 100.0);
  const std::array values{m.retiring_pct, m.bad_speculation_pct, m.frontend_bound_pct, m.backend_bound_pct};
  constexpr std::array names{"Retiring", "Bad Speculation", "Frontend Bound", "Backend Bound"};
  m.dominant_bound = names[static_cast<std::size_t>(std::max_element(values.begin(), values.end()) - values.begin())];
  return m;
}
std::optional<std::uint64_t> reference_probe_checksum(std::string_view name, std::uint64_t iterations) noexcept {
  const auto found = std::ranges::find(probes, name);
  if (found == probes.end()) return std::nullopt;
  const auto probe = static_cast<std::size_t>(found - probes.begin());
  std::uint64_t acc = 0;
  constexpr std::array<std::int64_t, 4> keys{-4, 0, 3, 9}, values{5, 7, 11, 13};
  for (std::uint64_t i = 0; i < iterations; ++i) {
    const std::uint64_t input = ((i * 17) & 255) * 0x0101010101010101ULL;
    std::uint64_t value = 0;
    if (probe == 0)
      value = (input & 15) == 5;
    else if (probe == 1)
      value = (input >> 60) == 5;
    else if (probe == 2 || probe == 3)
      value = (i & 1) == 0 ? input : ~input;
    else if (probe == 4)
      value = input / 97;
    else if (probe == 5)
      value = static_cast<std::uint32_t>(input) % 97;
    else if (probe == 7) {
      for (unsigned bit = 0; bit < 64; ++bit)
        if ((input >> bit) & 1) value += bit;
    } else if (probe == 6 || probe == 8 || probe == 9) {
      for (unsigned bit = 0; bit < 32; ++bit) value |= ((input >> (bit * 2)) & 1) << bit;
    } else if (probe == 10) {
      for (std::size_t k = 0; k < 4; ++k)
        if (keys[k] < static_cast<std::int64_t>(i & 7) - 2) value += static_cast<std::uint64_t>(values[k]);
    } else if (probe == 11) {
      value = UINT32_MAX;
      for (std::size_t k = 0; k < 4; ++k)
        if (keys[k] >= static_cast<std::int64_t>(i & 7) - 2) {
          value = k;
          break;
        }
    }
    acc = combine_probe_checksum(acc, i, value);
  }
  return acc;
}
namespace {
bool executable(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec) && !ec && ::access(path.c_str(), X_OK) == 0;
}
std::filesystem::path simulator() {
  if (const auto* specified = std::getenv("UARCH_GEM5_BIN")) {
    if (!*specified) return {};
    std::error_code ec;
    auto path = std::filesystem::absolute(specified, ec);
    return ec ? std::filesystem::path{} : path;
  }
  const auto* path = std::getenv("PATH");
  if (!path) return {};
  for (auto name : {"gem5.fast", "gem5.opt"}) {
    std::istringstream paths(path);
    std::string directory;
    while (std::getline(paths, directory, ':')) {
      auto candidate = std::filesystem::path(directory.empty() ? "." : directory) / name;
      if (executable(candidate)) return std::filesystem::absolute(candidate);
    }
  }
  return {};
}
struct Temporary {
  std::filesystem::path path;
  ~Temporary() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
}  // namespace
Result gem5_gate(const Context& context) {
  const auto cpus = gem5_cpu_models(context);
  if (cpus.empty()) return {"gem5-gate", Status::Fail, "invalid/empty gem5 profiles"};
  double override_budget = 0;
  if (const auto* env = std::getenv("UARCH_GEM5_MAX_CYCLES_PER_OP")) {
    auto value = number(env);
    if (!value || *value <= 0) return {"gem5-gate", Status::Fail, "invalid UARCH_GEM5_MAX_CYCLES_PER_OP"};
    override_budget = *value;
  }
  for (std::size_t i = 0; i < 7; ++i) {
    auto native = command({std::string(config::gem5_probe), "--probe", probes[i], "--iterations", "0000000064"});
    if (!native.ok || !guest_output(native.output, probes[i], 64))
      return {"gem5-gate", Status::Fail, "native guest checksum failed: " + std::string(probes[i]) + "; " + native.output};
  }
  const auto binary = simulator();
  if (binary.empty() && !std::getenv("UARCH_GEM5_BIN"))
    return {"gem5-gate", Status::NotRun,
            "gem5.opt/gem5.fast unavailable; verified native uarch_gem5_probe checksums on 7 mandatory scalar probes"};
  if (!executable(binary)) return {"gem5-gate", Status::Fail, "UARCH_GEM5_BIN is not an executable file: " + binary.string()};
  std::string pattern = "/tmp/uarch-gem5-XXXXXX";
  const auto* made = ::mkdtemp(pattern.data());
  if (!made) return {"gem5-gate", Status::Fail, "cannot create gem5 output directory"};
  Temporary temporary{made};
  std::ostringstream report;
  report << "parametric O3 models; not measured vendor silicon; simulator=" << binary.string() << '\n';
  struct Job {
    std::string cpu{};
    std::size_t probe{0};
    std::uint64_t width{0};
  };
  std::vector<Job> jobs;
  for (const auto& cpu : cpus) {
#if defined(__aarch64__)
    if (cpu != profiles[3]) return {"gem5-gate", Status::Fail, "profile ISA differs from guest ISA"};
    const std::size_t count = 7;
#else
    if (cpu == profiles[3]) return {"gem5-gate", Status::Fail, "profile ISA differs from guest ISA"};
    const std::size_t count = probes.size();
#endif
    const auto profile = command({"python3", (context.root / "scripts/gem5/o3_sim.py").string(), "--dump-profile-json", cpu});
    std::smatch match;
    const std::regex slot_pattern(R"("slot_width"\s*:\s*([0-9]+))");
    if (!profile.ok || !std::regex_search(profile.output, match, slot_pattern))
      return {"gem5-gate", Status::Fail, "missing profile slot_width: " + cpu};
    const auto width = integer(match[1].str());
    if (!width || !*width) return {"gem5-gate", Status::Fail, "invalid profile slot_width: " + cpu};
    for (std::size_t probe = 0; probe < count; ++probe) jobs.push_back({cpu, probe, *width});
  }
  auto simulate_job = [&](const Job& job) -> Result {
    const auto& cpu = job.cpu;
    const auto probe = job.probe;
    const auto width = job.width;
    std::ostringstream report;
    std::array<Gem5Counters, 3> samples{};
    bool unsupported = false;
    const std::string_view name = probes[probe];
    const bool extended = name == "bit_ones_sum" || name == "x86_v3_gather" || name == "x86_v3_slow_pdep_gather" ||
                          name == "uarch_arch_masked_sum" || name == "uarch_arch_first_ge";
    auto admission_failed = [&](std::string_view reason) {
      if (!extended) return false;
      report << cpu << ' ' << name << " Unsupported: " << reason << '\n';
      unsupported = true;
      return true;
    };
    constexpr std::array counts{512ULL, 2560ULL, 4608ULL};
    for (std::size_t run = 0; run < 3; ++run) {
      const auto out = temporary.path / (cpu + "-" + probes[probe] + "-" + std::to_string(run));
      std::error_code ec;
      std::filesystem::create_directories(out, ec);
      if (ec) return {"gem5-gate", Status::Fail, ec.message()};
      std::ostringstream n;
      n << std::setfill('0') << std::setw(10) << counts[run];
      std::vector<std::string> args{"env",
                                    "GLIBC_TUNABLES=glibc.pthread.rseq=0",
                                    binary.string(),
                                    "--outdir=" + out.string(),
                                    (context.root / "scripts/gem5/o3_sim.py").string(),
                                    "--cpu",
                                    cpu,
                                    "--binary",
                                    std::string(config::gem5_probe),
                                    "--probe",
                                    probes[probe],
                                    "--iterations",
                                    n.str()};
      if (executable("/usr/bin/timeout")) args.insert(args.begin(), {"/usr/bin/timeout", "120s"});
      const auto result = command(args);
      if (!result.ok) {
        if (admission_failed("simulator execution")) break;
        return {"gem5-gate", Status::Fail, "simulation execution failed: " + std::string(name) + "; " + result.output};
      }
      if (!guest_output(result.output, name, counts[run])) {
        if (admission_failed("guest checksum")) break;
        return {"gem5-gate", Status::Fail, "simulated checksum mismatch: " + std::string(name)};
      }
      auto parsed = parse_gem5_stats(read(out / "stats.txt"));
      if (!parsed) {
        if (admission_failed("invalid O3 statistics")) break;
        return {"gem5-gate", Status::Fail, "missing/invalid O3 statistics: " + std::string(name)};
      }
      samples[run] = *parsed;
    }
    if (unsupported) return {"gem5-gate", Status::Pass, report.str()};
    auto delta = check_gem5_linearity(samples[0], counts[0], samples[1], counts[1], samples[2], counts[2]);
    if (!delta) return {"gem5-gate", Status::Fail, "non-stationary O3 ROI: " + std::string(probes[probe])};
    if (!delta->has_structure) return {"gem5-gate", Status::Fail, "missing structural O3 counters: " + std::string(name)};
    if (!delta->has_rename_slots) return {"gem5-gate", Status::NotRun, "O3 rename-slot statistics unavailable"};
    auto metrics = compute_gem5_metrics(*delta, counts[2] - counts[1], width);
    if (!metrics) return {"gem5-gate", Status::Fail, "inconsistent O3 slot accounting: " + std::string(probes[probe])};
    const double budget = override_budget > 0 ? override_budget : ((probe == 6 || probe == 7) ? 256.0 : 32.0);
    if (metrics->cycles_per_op > budget || metrics->mem_order_violations != 0 ||
        (probe < 4 && metrics->branch_mispredicts_per_op > 0.005))
      return {"gem5-gate", Status::Fail,
              "O3 structural/cycle budget exceeded: " + std::string(probes[probe]) + " cycles/op=" +
                  std::to_string(metrics->cycles_per_op) + " memory=" + std::to_string(metrics->mem_order_violations) +
                  " branch/op=" + std::to_string(metrics->branch_mispredicts_per_op)};
    report << cpu << ' ' << probes[probe] << " Pass cycles/op=" << metrics->cycles_per_op << " budget=" << budget
           << " insts/op=" << metrics->insts_per_op << " uops/op=" << metrics->uops_per_op << " IPC=" << metrics->ipc
           << " branch-misses/op=" << metrics->branch_mispredicts_per_op
           << " dcache-misses/op=" << metrics->dcache_misses_per_op << " mem-order-violations=" << metrics->mem_order_violations
           << " ROB/IQ/LQ/SQ-full/op=" << metrics->rob_full_per_op << '/' << metrics->iq_full_per_op << '/'
           << metrics->lq_full_per_op << '/' << metrics->sq_full_per_op
           << " Retiring/BadSpec/Frontend/Backend=" << metrics->retiring_pct << '/' << metrics->bad_speculation_pct << '/'
           << metrics->frontend_bound_pct << '/' << metrics->backend_bound_pct << " mode=" << metrics->attribution_mode
           << " dominant=" << metrics->dominant_bound << '\n';
    return {"gem5-gate", Status::Pass, report.str()};
  };
  std::vector<Result> results(jobs.size());
  std::atomic<std::size_t> next{0};
  std::vector<std::thread> workers;
  for (std::size_t i = 0; i < std::min<std::size_t>(jobs.size(), 16); ++i) {
    workers.emplace_back([&] {
      for (;;) {
        const auto index = next.fetch_add(1);
        if (index >= jobs.size()) break;
        results[index] = simulate_job(jobs[index]);
      }
    });
  }
  for (auto& worker : workers) worker.join();
  for (const auto& result : results) {
    if (result.status != Status::Pass) return result;
    report << result.detail;
  }
  return {"gem5-gate", Status::Pass, report.str()};
}
}  // namespace uarch::verify
