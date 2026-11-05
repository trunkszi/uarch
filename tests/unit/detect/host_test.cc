#include <limits>
#include <uarch/detect/host.hpp>
namespace detect_fixture {
uarch::detect::Snapshot IntelV4();
}
using namespace uarch::detect;
namespace {
struct FakeBackend {
  CpuList before{3, 2048}, after{3, 2048};
  unsigned affinity_reads = 0, local_reads = 0, remote_reads = 0, xcr_reads = 0;
  bool remote_failure = false, missing_xsave = false;
  auto affinity_cpus() noexcept -> std::expected<CpuList, AuditError> { return affinity_reads++ == 0 ? before : after; }
  auto local_snapshot() noexcept -> std::expected<Snapshot, AuditError> {
    ++local_reads;
    return detect_fixture::IntelV4();
  }
  auto remote_snapshot(std::uint32_t) noexcept -> std::expected<Snapshot, AuditError> {
    ++remote_reads;
    if (remote_failure) return std::unexpected(AuditError::Unavailable);
    auto source = detect_fixture::IntelV4();
    if (missing_xsave) source.signature.ecx &= ~(1U << 27);
    return source;
  }
  auto thread_xcr0() noexcept -> std::optional<std::uint64_t> {
    ++xcr_reads;
    return 0xe6;
  }
};
}  // namespace
int main() {
  static_assert(Observed<int>::known(9).is_known() && Observed<int>::known(9).value_or(0) == 9);
  static_assert(Observed<int>::unknown().is_unknown() && Observed<int>::unsupported().is_unsupported());
  auto unknown = Observed<int>::unknown();
  if (unknown.get() != nullptr || unknown.value_or(3) != 3) return 1;
  auto known = Observed<int>::known(7);
  if (!known.get() || *known.get() != 7) return 2;
  for (auto text : std::array<std::string_view, 8>{"", "2-1", "1-2-3", "65536", "-1", "1,", "a", "0-999999999999999999999999"})
    if (detail::parse_cpu_list(text)) return 3;
  auto list = detail::parse_cpu_list("1,3-5,3\n");
  if (!list || *list != CpuList{1, 3, 4, 5}) return 4;
  std::vector<unsigned long> words(8192 / sizeof(unsigned long), 0);
  words[0] = 1UL << 3;
  words.back() = 1UL << (sizeof(unsigned long) * 8 - 1);
  auto sparse = detail::parse_affinity_mask(words);
  if (!sparse || *sparse != CpuList{3, 65535}) return 5;
  words.push_back(0);
  if (detail::parse_affinity_mask(words)) return 6;
  auto huge = detail::parse_huge_pages("HugePages_Total: 4\nHugePages_Free: 3\nHugepagesize: 2048 kB\n");
  if (!huge || huge->size_kib != 2048 || huge->total != 4 || huge->free != 3 ||
      detail::parse_huge_pages("HugePages_Free: -1\n"))
    return 7;
  FakeBackend backend;
  auto domain = detail::audit_execution_domain(backend);
  if (!domain || domain->cpus != backend.before || backend.affinity_reads != 2 || backend.remote_reads != 2 ||
      backend.local_reads != 0 || backend.xcr_reads != 1)
    return 8;
  backend = FakeBackend{};
  backend.after = {3};
  if (detail::audit_execution_domain(backend) || backend.affinity_reads != 2) return 9;
  backend = FakeBackend{};
  backend.remote_failure = true;
  if (detail::audit_execution_domain(backend) || backend.affinity_reads != 2 || backend.xcr_reads != 0) return 10;
  backend = FakeBackend{};
  backend.before = backend.after = {3};
  domain = detail::audit_execution_domain(backend);
  if (!domain || backend.local_reads != 1 || backend.remote_reads != 0) return 11;
  backend = FakeBackend{};
  backend.missing_xsave = true;
  domain = detail::audit_execution_domain(backend);
  if (!domain || backend.xcr_reads != 0 || domain->sources[0].xcr0_value) return 12;
  auto before = affinity_cpus();
  auto report = HostHardwareAudit::probe();
  auto after = affinity_cpus();
  if (before.has_value() != after.has_value() || (before && *before != *after)) return 13;
  if (auto lines = report.cache_lines.get())
    for (auto line : *lines)
      if (line.level == 0 || line.bytes == 0) return 14;
  if (auto siblings = report.smt_siblings.get())
    for (const auto& item : *siblings)
      if (item.siblings.empty() || !std::ranges::is_sorted(item.siblings)) return 15;
  return 0;
}
