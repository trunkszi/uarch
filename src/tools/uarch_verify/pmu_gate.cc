#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
#include <uarch/detect/policy.hpp>
#if defined(__linux__)
#include <linux/perf_event.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#endif
#include "verify.hpp"

extern "C" {
std::uint64_t flow_select_register_u64(bool, std::uint64_t, std::uint64_t);
bool bitword_low_matches(std::uint64_t);
std::uint64_t fast_divisor_u64_div(uarch::arithmetic::FastDivisorU64, std::uint64_t);
std::uint32_t fast_divisor_u32_rem(uarch::arithmetic::FastDivisorU32, std::uint32_t);
#if defined(__x86_64__)
std::uint64_t x86_v3_gather(std::uint64_t, std::uint64_t);
std::uint64_t x86_v3_slow_pdep_gather(std::uint64_t, std::uint64_t);
std::int64_t uarch_arch_masked_sum(const std::int64_t*, const std::int64_t*, std::int64_t);
#endif
}
namespace uarch::verify {
namespace {
std::string_view trim(std::string_view s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  return begin == std::string_view::npos ? std::string_view{} : s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}
std::optional<double> number(std::string_view s) {
  s = trim(s);
  double v = 0;
  const auto parsed = std::from_chars(s.data(), s.data() + s.size(), v);
  if (s.empty() || parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size() || !std::isfinite(v) || v < 0)
    return std::nullopt;
  return v;
}
}  // namespace
TmaMetrics compute_tma_metrics(const PmuCounters& c, std::uint64_t operations) noexcept {
  TmaMetrics m;
  if (!operations) return m;
  const auto ops = static_cast<double>(operations), cycles = std::max(1.0, c.cycles);
  const auto slots = std::max(1.0, c.cycles * static_cast<double>(c.slot_width));
  m.cycles_per_op = c.cycles / ops;
  m.instructions_per_op = c.instructions / ops;
  m.ipc = c.instructions / cycles;
  m.branch_misses_per_op = c.branch_misses / ops;
  std::array<double, 4> raw{};
  if (c.has_raw_slots)
    raw = {c.retired_ops / slots, std::max(0.0, (c.dispatched_ops - c.retired_ops) / slots), c.frontend_no_ops_slots / slots,
           c.backend_stall_slots / slots};
  else
    raw = {std::clamp(c.instructions / slots, 0.0, 1.0), std::clamp(c.branch_misses * 16.0 / cycles, 0.0, 1.0),
           std::clamp(c.stalled_frontend / cycles, 0.0, 1.0), std::clamp(c.stalled_backend / cycles, 0.0, 1.0)};
  double total = 0;
  for (auto v : raw) total += v;
  if (total > 0) {
    m.retiring_pct = raw[0] * 100 / total;
    m.bad_speculation_pct = raw[1] * 100 / total;
    m.frontend_bound_pct = raw[2] * 100 / total;
    m.backend_bound_pct = raw[3] * 100 / total;
    constexpr std::array names{"Retiring", "Bad Speculation", "Frontend Bound", "Backend Bound"};
    m.dominant_bound = names[static_cast<std::size_t>(std::max_element(raw.begin(), raw.end()) - raw.begin())];
  }
  return m;
}
std::optional<PmuCounters> parse_perf_stat_csv(std::string_view text) noexcept {
  PmuCounters c;
  bool instructions = false, misses = false;
  std::array<bool, 10> seen{};
  std::array<bool, 4> raw_available{};
  while (!text.empty()) {
    const auto end = text.find('\n');
    auto line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    std::array<std::string_view, 5> fields{};
    for (auto& field : fields) {
      const auto split = line.find(';');
      field = trim(line.substr(0, split));
      line = split == std::string_view::npos ? std::string_view{} : line.substr(split + 1);
    }
    auto event = fields[2];
    if (event.ends_with(":u")) event.remove_suffix(2);
    constexpr std::array names{
        "cycles", "instructions", "branches", "branch-misses", "stalled-cycles-frontend", "stalled-cycles-backend",
        "r00c1",  "r07aa",        "r01a0",    "r1ea0"};
    auto found = std::ranges::find(names, event);
    if (found == names.end()) continue;
    const auto index = static_cast<std::size_t>(found - names.begin());
    if (seen[index]) return std::nullopt;
    seen[index] = true;
    const auto value = number(fields[0]);
    if (!value) {
      if (index < 4 || fields[0].find('<') == std::string_view::npos) return std::nullopt;
      continue;
    }
    auto percent = fields[4];
    if (percent.ends_with('%')) percent.remove_suffix(1);
    const auto active = number(percent);
    if (!active || *active > 100) return std::nullopt;
    c.min_active_ratio = std::min(c.min_active_ratio, *active / 100);
    std::array<double*, 10> values{
        &c.cycles,          &c.instructions, &c.branches,       &c.branch_misses,         &c.stalled_frontend,
        &c.stalled_backend, &c.retired_ops,  &c.dispatched_ops, &c.frontend_no_ops_slots, &c.backend_stall_slots};
    *values[index] = *value;
    if (index >= 6) raw_available[index - 6] = *active >= 99;
    instructions |= index == 1;
    misses |= index == 3;
  }
  if (!instructions || !misses) return std::nullopt;
  c.has_raw_slots = std::ranges::all_of(raw_available, [](bool available) { return available; });
  return c;
}
namespace {
constexpr std::array probe_names{"flow_select_register_u64", "bitword_low_matches", "fast_divisor_u64_div",
                                 "fast_divisor_u32_rem",     "x86_v3_gather",       "x86_v3_slow_pdep_gather",
                                 "uarch_arch_masked_sum"};
template <std::size_t Probe, bool Reference>
std::uint64_t probe_loop(std::uint64_t iterations) {
  constexpr auto divisor64 = arithmetic::divisor_u64<97>();
  constexpr auto divisor32 = arithmetic::divisor_u32<97>();
  const std::array<std::int64_t, 4> keys{-4, 0, 3, 9}, values{5, 7, 11, 13};
  std::uint64_t checksum = 0;
  for (std::uint64_t i = 0; i < iterations; ++i) {
    auto input = i;
    asm volatile("" : "+r"(input) : : "memory");
    std::uint64_t value = 0;
    if constexpr (Reference) {
      if constexpr (Probe == 0) value = (input & 1) == 0 ? input : ~input;
      if constexpr (Probe == 1) value = (input & 15) == 5;
      if constexpr (Probe == 2) value = input / 97;
      if constexpr (Probe == 3) value = static_cast<std::uint32_t>(input) % 97;
      if constexpr (Probe == 4 || Probe == 5) {
        for (unsigned bit = 0; bit < 32; ++bit) value |= ((input >> (bit * 2)) & 1) << bit;
      }
      if constexpr (Probe == 6) {
        for (std::size_t k = 0; k < keys.size(); ++k)
          if (keys[k] < static_cast<std::int64_t>(input & 7) - 2) value += static_cast<std::uint64_t>(values[k]);
      }
    } else {
      if constexpr (Probe == 0) value = flow_select_register_u64((input & 1) == 0, input, ~input);
      if constexpr (Probe == 1) value = bitword_low_matches(input);
      if constexpr (Probe == 2) value = fast_divisor_u64_div(divisor64, input);
      if constexpr (Probe == 3) value = fast_divisor_u32_rem(divisor32, static_cast<std::uint32_t>(input));
#if defined(__x86_64__)
      if constexpr (Probe == 4) value = x86_v3_gather(input, 0x5555555555555555ULL);
      if constexpr (Probe == 5) value = x86_v3_slow_pdep_gather(input, 0x5555555555555555ULL);
      if constexpr (Probe == 6)
        value = static_cast<std::uint64_t>(
            uarch_arch_masked_sum(keys.data(), values.data(), static_cast<std::int64_t>(input & 7) - 2));
#endif
    }
    asm volatile("" : "+r"(value));
    checksum = combine_probe_checksum(checksum, i, value);
  }
  return checksum;
}
template <bool Reference>
std::uint64_t run_loop(std::size_t probe, std::uint64_t iterations) {
  switch (probe) {
    case 0:
      return probe_loop<0, Reference>(iterations);
    case 1:
      return probe_loop<1, Reference>(iterations);
    case 2:
      return probe_loop<2, Reference>(iterations);
    case 3:
      return probe_loop<3, Reference>(iterations);
    case 4:
      return probe_loop<4, Reference>(iterations);
    case 5:
      return probe_loop<5, Reference>(iterations);
    case 6:
      return probe_loop<6, Reference>(iterations);
    default:
      return 0;
  }
}
#if defined(__linux__)
class AffinityGuard {
  std::vector<unsigned long> saved_{};

 public:
  bool pinned{false};
  AffinityGuard() {
    for (std::size_t bytes = 128; bytes <= 8192; bytes *= 2) {
      saved_.resize(bytes / sizeof(unsigned long));
      if (::sched_getaffinity(0, bytes, reinterpret_cast<cpu_set_t*>(saved_.data())) == 0) {
        const int cpu = ::sched_getcpu();
        if (cpu < 0 || static_cast<std::size_t>(cpu) >= bytes * 8) return;
        std::vector<unsigned long> single(saved_.size());
        const auto index = static_cast<std::size_t>(cpu);
        single[index / (sizeof(unsigned long) * 8)] = 1UL << (index % (sizeof(unsigned long) * 8));
        pinned = ::sched_setaffinity(0, bytes, reinterpret_cast<const cpu_set_t*>(single.data())) == 0;
        return;
      }
      if (errno != EINVAL) return;
    }
  }
  bool restore() {
    if (!pinned) return true;
    if (::sched_setaffinity(0, saved_.size() * sizeof(unsigned long), reinterpret_cast<const cpu_set_t*>(saved_.data())) != 0)
      return false;
    pinned = false;
    return true;
  }
  ~AffinityGuard() {
    if (!restore()) std::abort();
  }
};
struct CounterFd {
  int fd;
  explicit CounterFd(int descriptor) noexcept : fd(descriptor) {}
  CounterFd(const CounterFd&) = delete;
  CounterFd& operator=(const CounterFd&) = delete;
  ~CounterFd() {
    if (fd >= 0) ::close(fd);
  }
};
#endif
bool supported_v3() {
#if defined(__x86_64__)
  return detect::v3_supported(detect::HardwareCpuid{});
#else
  return false;
#endif
}
std::uint64_t host_slot_width() {
#if defined(__x86_64__)
  detect::HardwareCpuid source;
  const auto family = detect::family_model(source.cpuid(1, 0).eax).first;
  const auto vendor = detect::vendor(source);
  const auto name = std::string_view(vendor.data(), vendor.size());
  if (name == "AuthenticAMD") return family == 0x1a ? 8 : family == 0x19 ? 6 : 4;
  const auto model = detect::family_model(source.cpuid(1, 0).eax).second;
  if (name == "GenuineIntel" && family == 6 &&
      (model == 0x97 || model == 0x9a || model == 0xb7 || model == 0xba || model == 0x8f || model == 0xcf))
    return 6;
#endif
  return 4;
}
}  // namespace
Result run_pmu_probe(std::string_view name, std::uint64_t iterations) {
  const auto found = std::ranges::find(probe_names, name);
  if (found == probe_names.end() || !iterations) return {"pmu-gate", Status::Fail, "invalid probe or iterations"};
  const auto index = static_cast<std::size_t>(found - probe_names.begin());
#if defined(__linux__)
  AffinityGuard affinity;
  if (index >= 4 && (!affinity.pinned || !supported_v3()))
    return {"pmu-gate", Status::NotRun, "V3 execution domain unavailable"};
#else
  if (index >= 4) return {"pmu-gate", Status::NotRun, "V3 execution domain unavailable"};
#endif
  const auto checksum = run_loop<false>(index, iterations);
  if (checksum != run_loop<true>(index, iterations))
    return {"pmu-gate", Status::Fail, "probe checksum mismatch: " + std::string(name)};
  return {"pmu-gate", Status::Pass, std::string(name) + " checksum=" + std::to_string(checksum)};
}
Result pmu_gate(const Context& context) {
  auto iterations = context.pmu_iterations;
  if (const auto* env = std::getenv("UARCH_PMU_ITERATIONS"); env && !context.pmu_iterations_explicit) {
    const std::string_view text(env);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), iterations);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
      return {"pmu-gate", Status::Fail, "invalid UARCH_PMU_ITERATIONS"};
  }
  if (!iterations) return {"pmu-gate", Status::Fail, "iterations must be positive"};
#if !defined(__linux__)
  for (std::size_t i = 0; i < 4; ++i)
    if (run_loop<false>(i, iterations) != run_loop<true>(i, iterations))
      return {"pmu-gate", Status::Fail, "probe checksum mismatch"};
  return {"pmu-gate", Status::NotRun, "PMU unavailable on this OS; scalar probe checksums verified"};
#else
  AffinityGuard affinity;
  const std::size_t count = affinity.pinned && supported_v3() ? probe_names.size() : 4;
  std::string checksums;
  std::array<std::uint64_t, 7> expected{};
  for (std::size_t i = 0; i < count; ++i) {
    expected[i] = run_loop<true>(i, iterations);
    const auto actual = run_loop<false>(i, iterations);
    if (actual != expected[i]) return {"pmu-gate", Status::Fail, "probe checksum mismatch: " + std::string(probe_names[i])};
    checksums += std::string(probe_names[i]) + " checksum=" + std::to_string(actual) + "; ";
  }
  const auto width = host_slot_width();
  bool zen = false;
#if defined(__x86_64__)
  detect::HardwareCpuid source;
  const auto vendor = detect::vendor(source);
  const auto [family, model] = detect::family_model(source.cpuid(1, 0).eax);
  zen = std::string_view(vendor.data(), vendor.size()) == "AuthenticAMD" && zen_raw_slots_supported(family, model);
#endif
  std::ostringstream report;
  report << "iterations=" << iterations << "; slot_width=" << width
         << "; per-event deterministic replay; counts include loop/call overhead; normalized TMA estimates\n"
         << "probe cycles/op instructions/op IPC branch-misses/op Retiring% BadSpec% FrontendBound% BackendBound% model "
            "dominant\n";
  for (std::size_t probe = 0; probe < count; ++probe) {
    PmuCounters counters;
    counters.slot_width = width;
    std::array<double*, 10> values{&counters.cycles,
                                   &counters.instructions,
                                   &counters.branches,
                                   &counters.branch_misses,
                                   &counters.stalled_frontend,
                                   &counters.stalled_backend,
                                   &counters.retired_ops,
                                   &counters.dispatched_ops,
                                   &counters.frontend_no_ops_slots,
                                   &counters.backend_stall_slots};
    constexpr std::array<std::uint64_t, 10> configs{PERF_COUNT_HW_CPU_CYCLES,
                                                    PERF_COUNT_HW_INSTRUCTIONS,
                                                    PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
                                                    PERF_COUNT_HW_BRANCH_MISSES,
                                                    PERF_COUNT_HW_STALLED_CYCLES_FRONTEND,
                                                    PERF_COUNT_HW_STALLED_CYCLES_BACKEND,
                                                    0x00c1,
                                                    0x07aa,
                                                    0x01a0,
                                                    0x1ea0};
    bool external = false;
    std::array<bool, 4> raw_available{};
    for (std::size_t event = 0; event < (zen ? 10U : 6U); ++event) {
      perf_event_attr attr{};
      attr.size = sizeof(attr);
      attr.type = event < 6 ? PERF_TYPE_HARDWARE : PERF_TYPE_RAW;
      attr.config = configs[event];
      attr.disabled = 1;
      attr.exclude_kernel = 1;
      attr.exclude_hv = 1;
      attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
      CounterFd counter{static_cast<int>(::syscall(SYS_perf_event_open, &attr, 0, -1, -1, PERF_FLAG_FD_CLOEXEC))};
      if (counter.fd < 0) {
        if (event >= 4) continue;
        const int failure = errno;
        if (failure != EACCES && failure != EPERM && failure != ENOENT && failure != EOPNOTSUPP && failure != EINVAL &&
            failure != ENODEV && failure != EBUSY && failure != ENOSPC)
          return {"pmu-gate", Status::Fail, "perf_event_open error=" + std::to_string(failure)};
        std::error_code executable_error;
        const auto executable = std::filesystem::read_symlink("/proc/self/exe", executable_error);
        auto runner = executable.parent_path() / "uarch_verify";
        if (!std::filesystem::is_regular_file(runner, executable_error))
          runner = executable.parent_path().parent_path() / "uarch_verify";
        auto perf = command({"perf", "stat", "-x", ";", "-e", "cycles:u,instructions:u,branches:u,branch-misses:u", "--",
                             runner.string(), "__pmu-probe", "--pmu-probe", probe_names[probe], "--pmu-iterations",
                             std::to_string(iterations)});
        if (perf.ok && (perf.output.find(std::string(probe_names[probe]) + " checksum=" + std::to_string(expected[probe])) ==
                            std::string::npos ||
                        perf.output.find(R"json("gate":"pmu-gate","status":"Pass")json") == std::string::npos))
          return {"pmu-gate", Status::Fail, "external perf probe did not verify its checksum"};
        auto parsed = perf.ok ? parse_perf_stat_csv(perf.output) : std::nullopt;
        if (!parsed || parsed->cycles <= 0) {
          const auto paranoid_text = read("/proc/sys/kernel/perf_event_paranoid");
          const auto paranoid = trim(paranoid_text);
          return {
              "pmu-gate", Status::NotRun,
              "PMU counters unavailable (perf_event_paranoid=" + std::string(paranoid) +
                  "); probe checksums verified; set kernel.perf_event_paranoid<=1 or grant CAP_PERFMON for hardware PMU+TMA; " +
                  checksums + "perf: " + perf.output};
        }
        counters = *parsed;
        counters.slot_width = width;
        external = true;
        break;
      }
      if (::ioctl(counter.fd, PERF_EVENT_IOC_RESET, 0) < 0 || ::ioctl(counter.fd, PERF_EVENT_IOC_ENABLE, 0) < 0)
        return {"pmu-gate", Status::NotRun, "cannot enable PMU event; probe checksums verified"};
      const auto checksum = run_loop<false>(probe, iterations);
      if (::ioctl(counter.fd, PERF_EVENT_IOC_DISABLE, 0) < 0) return {"pmu-gate", Status::Fail, "cannot disable PMU event"};
      if (checksum != expected[probe]) return {"pmu-gate", Status::Fail, "measured probe checksum mismatch"};
      struct {
        std::uint64_t value, enabled, running;
      } sample{};
      if (::read(counter.fd, &sample, sizeof(sample)) != sizeof(sample) || !sample.enabled || !sample.running)
        return {"pmu-gate", Status::NotRun, "PMU event did not run; probe checksums verified"};
      const auto ratio = static_cast<double>(sample.running) / static_cast<double>(sample.enabled);
      counters.min_active_ratio = std::min(counters.min_active_ratio, ratio);
      *values[event] = static_cast<double>(sample.value);
      if (event >= 6) raw_available[event - 6] = ratio >= 0.99;
    }
    if (counters.min_active_ratio < 0.99)
      return {"pmu-gate", Status::NotRun, "multiplexed PMU counter; probe checksums verified"};
    if (counters.cycles <= 0 || counters.instructions <= 0)
      return {"pmu-gate", Status::NotRun, "zero PMU counts; probe checksums verified"};
    counters.has_raw_slots =
        counters.has_raw_slots || std::ranges::all_of(raw_available, [](bool available) { return available; });
    auto m = compute_tma_metrics(counters, iterations);
    if (probe < 2 && m.branch_misses_per_op > 0.005)
      return {"pmu-gate", Status::Fail, "branch-miss budget exceeded: " + std::string(probe_names[probe])};
    report << probe_names[probe] << ' ' << std::fixed << std::setprecision(4) << m.cycles_per_op << ' ' << m.instructions_per_op
           << ' ' << m.ipc << ' ' << m.branch_misses_per_op << ' ' << m.retiring_pct << ' ' << m.bad_speculation_pct << ' '
           << m.frontend_bound_pct << ' ' << m.backend_bound_pct << ' '
           << (counters.has_raw_slots ? "raw-slot " : "generic-estimate ") << m.dominant_bound
           << (external ? " (external perf: startup included)" : "") << '\n';
  }
  if (!affinity.restore()) return {"pmu-gate", Status::Fail, "cannot restore affinity"};
  return {"pmu-gate", Status::Pass, report.str() + "checksums verified: " + checksums};
#endif
}
}  // namespace uarch::verify
