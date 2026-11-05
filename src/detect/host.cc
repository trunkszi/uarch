#include <array>
#include <bit>
#include <cerrno>
#include <charconv>
#include <limits>
#include <memory>
#include <string>
#include <uarch/detect/host.hpp>
#if defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/auxv.h>
#include <unistd.h>
#endif

namespace uarch::detect {
namespace {
std::string_view Trim(std::string_view text) noexcept {
  constexpr std::string_view kSpace = " \t\r\n";
  auto first = text.find_first_not_of(kSpace);
  if (first == std::string_view::npos) return {};
  return text.substr(first, text.find_last_not_of(kSpace) - first + 1);
}
std::expected<std::uint64_t, AuditError> Number(std::string_view text) noexcept {
  text = Trim(text);
  if (text.empty()) return std::unexpected(AuditError::Malformed);
  std::uint64_t value{};
  auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    return std::unexpected(AuditError::Malformed);
  return value;
}
#if defined(__linux__)
class File {
  int fd_;

 public:
  explicit File(int fd) noexcept : fd_(fd) {}
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  ~File() noexcept {
    if (fd_ >= 0) ::close(fd_);
  }
  [[nodiscard]] int get() const noexcept { return fd_; }
};
std::expected<std::string, AuditError> ReadFile(const std::string& path) noexcept {
  File file{::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
  if (file.get() < 0) return std::unexpected(AuditError::Unavailable);
  std::string result;
  std::array<char, 4096> buffer{};
  for (;;) {
    auto count = ::read(file.get(), buffer.data(), buffer.size());
    if (count < 0) {
      if (errno == EINTR) continue;
      return std::unexpected(AuditError::Unavailable);
    }
    if (count == 0) return result;
    if (result.size() + static_cast<std::size_t>(count) > 1024 * 1024) return std::unexpected(AuditError::Malformed);
    result.append(buffer.data(), static_cast<std::size_t>(count));
  }
}
std::expected<std::uint64_t, AuditError> ReadNumber(const std::string& path) noexcept {
  auto text = ReadFile(path);
  if (!text) return std::unexpected(text.error());
  return Number(*text);
}
std::expected<std::vector<CacheLine>, AuditError> CacheLines(const CpuList& cpus) noexcept {
  std::vector<CacheLine> lines;
  for (auto cpu : cpus) {
    std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache";
    auto close_dir = [](DIR* directory) noexcept {
      if (directory) ::closedir(directory);
    };
    std::unique_ptr<DIR, decltype(close_dir)> directory{::opendir(base.c_str()), close_dir};
    if (!directory) return std::unexpected(AuditError::Unavailable);
    auto start = lines.size();
    for (;;) {
      errno = 0;
      auto* entry = ::readdir(directory.get());
      if (!entry) {
        if (errno != 0) return std::unexpected(AuditError::Unavailable);
        break;
      }
      std::string_view name{entry->d_name};
      if (!name.starts_with("index")) continue;
      if (lines.size() - start >= 64) return std::unexpected(AuditError::Malformed);
      auto level = ReadNumber(base + "/" + std::string(name) + "/level");
      auto bytes = ReadNumber(base + "/" + std::string(name) + "/coherency_line_size");
      if (!level || !bytes) return std::unexpected(AuditError::Unavailable);
      if (*level == 0 || *level > 255 || *bytes == 0 || *bytes > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(AuditError::Malformed);
      lines.push_back({cpu, static_cast<std::uint8_t>(*level), static_cast<std::uint32_t>(*bytes)});
    }
    if (lines.size() == start) return std::unexpected(AuditError::Unavailable);
  }
  std::ranges::sort(lines);
  return lines;
}
std::expected<std::vector<SmtSiblings>, AuditError> Siblings(const CpuList& cpus) noexcept {
  std::vector<SmtSiblings> result;
  for (auto cpu : cpus) {
    auto text = ReadFile("/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/thread_siblings_list");
    if (!text) return std::unexpected(text.error());
    auto list = detail::parse_cpu_list(*text);
    if (!list) return std::unexpected(list.error());
    result.push_back({cpu, std::move(*list)});
  }
  return result;
}
#if defined(__x86_64__)
std::expected<CpuidRegisters, AuditError> RemoteRegisters(const File& file, std::uint32_t leaf) noexcept {
  std::array<unsigned char, 16> bytes{};
  ssize_t count;
  do {
    count = ::pread(file.get(), bytes.data(), bytes.size(), static_cast<off_t>(leaf));
  } while (count < 0 && errno == EINTR);
  if (count < 0) return std::unexpected(AuditError::Unavailable);
  if (count != 16) return std::unexpected(AuditError::Malformed);
  std::array<std::uint32_t, 4> values{};
  for (unsigned word = 0; word < 4; ++word)
    for (unsigned byte = 0; byte < 4; ++byte) values[word] |= static_cast<std::uint32_t>(bytes[word * 4 + byte]) << (byte * 8);
  return CpuidRegisters{values[0], values[1], values[2], values[3]};
}
std::expected<Snapshot, AuditError> RemoteSnapshot(std::uint32_t cpu) noexcept {
  File file{::open(("/dev/cpu/" + std::to_string(cpu) + "/cpuid").c_str(), O_RDONLY | O_CLOEXEC)};
  if (file.get() < 0) return std::unexpected(AuditError::Unavailable);
  Snapshot source;
  auto basic = RemoteRegisters(file, 0), extended = RemoteRegisters(file, 0x80000000);
  if (!basic || !extended) return std::unexpected(!basic ? basic.error() : extended.error());
  source.basic = *basic;
  source.extended = *extended;
  for (auto leaf : std::array<std::uint32_t, 4>{1, 7, 0x80000001, 0x80000007}) {
    if (!available(source, leaf)) continue;
    auto value = RemoteRegisters(file, leaf);
    if (!value) return std::unexpected(value.error());
    switch (leaf) {
      case 1:
        source.signature = *value;
        break;
      case 7:
        source.structured = *value;
        break;
      case 0x80000001:
        source.extended_features = *value;
        break;
      case 0x80000007:
        source.invariant = *value;
        break;
      default:
        break;
    }
  }
  return source;
}
#endif
#endif
}  // namespace

namespace detail {
std::expected<CpuList, AuditError> parse_cpu_list(std::string_view text) noexcept {
  text = Trim(text);
  if (text.empty()) return std::unexpected(AuditError::Malformed);
  CpuList result;
  for (;;) {
    auto comma = text.find(',');
    auto part = text.substr(0, comma);
    auto dash = part.find('-');
    auto first = Number(part.substr(0, dash));
    auto last = dash == std::string_view::npos ? first : Number(part.substr(dash + 1));
    if (!first || !last || *first > *last || *last > 65535 || *last - *first + 1 > 65536 - result.size())
      return std::unexpected(AuditError::Malformed);
    for (auto cpu = *first; cpu <= *last; ++cpu) result.push_back(static_cast<std::uint32_t>(cpu));
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  std::ranges::sort(result);
  auto tail = std::ranges::unique(result);
  result.erase(tail.begin(), tail.end());
  return result;
}
std::expected<CpuList, AuditError> parse_affinity_mask(std::span<const unsigned long> mask) noexcept {
  if (mask.size() > 8192 / sizeof(unsigned long)) return std::unexpected(AuditError::Malformed);
  CpuList result;
  for (std::size_t word = 0; word < mask.size(); ++word)
    for (unsigned bit = 0; bit < sizeof(unsigned long) * 8; ++bit) {
      if ((mask[word] & (1UL << bit)) != 0)
        result.push_back(static_cast<std::uint32_t>(word * sizeof(unsigned long) * 8 + bit));
    }
  if (result.empty()) return std::unexpected(AuditError::Unavailable);
  return result;
}
std::expected<HugePageConfig, AuditError> parse_huge_pages(std::string_view text) noexcept {
  std::array<std::optional<std::uint64_t>, 3> values;
  constexpr std::array<std::string_view, 3> kKeys{"Hugepagesize:", "HugePages_Total:", "HugePages_Free:"};
  while (!text.empty()) {
    auto newline = text.find('\n');
    auto line = text.substr(0, newline);
    for (std::size_t i = 0; i < 3; ++i)
      if (line.starts_with(kKeys[i])) {
        if (values[i]) return std::unexpected(AuditError::Malformed);
        auto number = Trim(line.substr(kKeys[i].size()));
        auto space = number.find_first_of(" \t");
        auto parsed = Number(number.substr(0, space));
        if (!parsed) return std::unexpected(parsed.error());
        values[i] = *parsed;
      }
    if (newline == std::string_view::npos) break;
    text.remove_prefix(newline + 1);
  }
  if (!values[0] || !values[1] || !values[2] || *values[0] == 0 || *values[2] > *values[1])
    return std::unexpected(AuditError::Malformed);
  return HugePageConfig{*values[0], *values[1], *values[2]};
}
}  // namespace detail

std::expected<CpuList, AuditError> affinity_cpus() noexcept {
#if defined(__linux__)
  for (auto bytes : std::array<std::size_t, 7>{128, 256, 512, 1024, 2048, 4096, 8192}) {
    std::vector<unsigned long> mask(bytes / sizeof(unsigned long), 0);
    auto* buffer = static_cast<cpu_set_t*>(static_cast<void*>(mask.data()));
    if (::sched_getaffinity(0, bytes, buffer) == 0) return detail::parse_affinity_mask(mask);
    if (errno != EINVAL) return std::unexpected(AuditError::Unavailable);
  }
#endif
  return std::unexpected(AuditError::Unavailable);
}
std::expected<ExecutionDomain, AuditError> execution_domain() noexcept {
#if defined(__linux__) && defined(__x86_64__)
  struct RealBackend {
    static std::expected<CpuList, AuditError> affinity_cpus() noexcept { return uarch::detect::affinity_cpus(); }
    static std::expected<Snapshot, AuditError> local_snapshot() noexcept { return Snapshot::from_source(HardwareCpuid{}); }
    static std::expected<Snapshot, AuditError> remote_snapshot(std::uint32_t cpu) noexcept { return RemoteSnapshot(cpu); }
    static std::optional<std::uint64_t> thread_xcr0() noexcept { return HardwareCpuid{}.xcr0(); }
  } backend;
  return detail::audit_execution_domain(backend);
#else
  return std::unexpected(AuditError::Unavailable);
#endif
}
HostReport HostHardwareAudit::probe() noexcept {
  HostReport report;
#if defined(__linux__)
  auto cpus = affinity_cpus();
  if (cpus) {
    report.affinity_cpus = Observed<CpuList>::known(*cpus);
    auto lines = CacheLines(*cpus);
    if (lines) report.cache_lines = Observed<std::vector<CacheLine>>::known(std::move(*lines));
    auto siblings = Siblings(*cpus);
    if (siblings) report.smt_siblings = Observed<std::vector<SmtSiblings>>::known(std::move(*siblings));
  }
  auto smt = ReadNumber("/sys/devices/system/cpu/smt/active");
  if (smt && *smt <= 1) report.smt_active = Observed<bool>::known(*smt != 0);
  auto meminfo = ReadFile("/proc/meminfo");
  if (meminfo) {
    auto huge = detail::parse_huge_pages(*meminfo);
    if (huge) report.huge_pages = Observed<HugePageConfig>::known(*huge);
  }
#if defined(__x86_64__)
  auto domain = execution_domain();
  if (domain && cpus && domain->cpus == *cpus && !domain->sources.empty()) {
    bool homogeneous = true, tsc = true;
    for (const auto& source : domain->sources) {
      homogeneous &= same_isa(domain->sources.front(), source);
      tsc &= available(source, 0x80000007) && (source.invariant.edx & (1U << 8)) != 0;
    }
    report.isa_homogeneous = Observed<bool>::known(homogeneous);
    report.invariant_tsc = Observed<bool>::known(tsc);
  }
#elif defined(__aarch64__)
  if ((getauxval(AT_HWCAP) & (1UL << 1)) != 0) report.isa_homogeneous = Observed<bool>::known(true);
  report.invariant_tsc = Observed<bool>::unsupported();
#else
  report.isa_homogeneous = Observed<bool>::unsupported();
  report.invariant_tsc = Observed<bool>::unsupported();
#endif
  auto after = affinity_cpus();
  if (!cpus || !after || *cpus != *after) {
    report.affinity_cpus = Observed<CpuList>::unknown();
    report.cache_lines = Observed<std::vector<CacheLine>>::unknown();
    report.smt_siblings = Observed<std::vector<SmtSiblings>>::unknown();
    report.isa_homogeneous = Observed<bool>::unknown();
    report.invariant_tsc = Observed<bool>::unknown();
  }
#else
  report.affinity_cpus = Observed<CpuList>::unsupported();
  report.isa_homogeneous = Observed<bool>::unsupported();
  report.cache_lines = Observed<std::vector<CacheLine>>::unsupported();
  report.smt_active = Observed<bool>::unsupported();
  report.smt_siblings = Observed<std::vector<SmtSiblings>>::unsupported();
  report.huge_pages = Observed<HugePageConfig>::unsupported();
  report.invariant_tsc = Observed<bool>::unsupported();
#endif
  return report;
}
}  // namespace uarch::detect
