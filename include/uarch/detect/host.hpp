/** @file @brief Read-only host observations and complete execution-domain snapshots. */
#pragma once
#include <algorithm>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <uarch/detect/cpuid.hpp>
#include <uarch/detect/error.hpp>
#include <utility>
#include <vector>

namespace uarch::detect {
enum class ObservedState : std::uint8_t { Known, Unknown, Unsupported };
/** @brief Absence of evidence never silently means a supported feature is false. */
template <class T>
class Observed {
  ObservedState state_{ObservedState::Unknown};
  std::optional<T> value_{};
  constexpr Observed(ObservedState state, std::optional<T> value) noexcept : state_(state), value_(std::move(value)) {}

 public:
  constexpr Observed() noexcept = default;
  [[nodiscard]] static constexpr Observed known(T value) noexcept { return {ObservedState::Known, std::move(value)}; }
  [[nodiscard]] static constexpr Observed unknown() noexcept { return {ObservedState::Unknown, std::nullopt}; }
  [[nodiscard]] static constexpr Observed unsupported() noexcept { return {ObservedState::Unsupported, std::nullopt}; }
  [[nodiscard]] constexpr bool is_known() const noexcept { return state_ == ObservedState::Known; }
  [[nodiscard]] constexpr bool is_unknown() const noexcept { return state_ == ObservedState::Unknown; }
  [[nodiscard]] constexpr bool is_unsupported() const noexcept { return state_ == ObservedState::Unsupported; }
  [[nodiscard]] constexpr const T* get() const noexcept { return is_known() ? std::addressof(*value_) : nullptr; }
  [[nodiscard]] constexpr T value_or(T fallback) const noexcept { return is_known() ? *value_ : std::move(fallback); }
  constexpr bool operator==(const Observed&) const noexcept = default;
};
struct CacheLine {
  std::uint32_t cpu{};
  std::uint8_t level{};
  std::uint32_t bytes{};
  constexpr auto operator<=>(const CacheLine&) const noexcept = default;
};
struct HugePageConfig {
  std::uint64_t size_kib{}, total{}, free{};
  constexpr bool operator==(const HugePageConfig&) const noexcept = default;
};
struct SmtSiblings {
  std::uint32_t cpu{};
  std::vector<std::uint32_t> siblings{};
  bool operator==(const SmtSiblings&) const noexcept = default;
};
using CpuList = std::vector<std::uint32_t>;
struct HostReport {
  Observed<CpuList> affinity_cpus{};
  Observed<bool> isa_homogeneous{};
  Observed<std::vector<CacheLine>> cache_lines{};
  Observed<bool> smt_active{};
  Observed<std::vector<SmtSiblings>> smt_siblings{};
  Observed<HugePageConfig> huge_pages{};
  Observed<bool> invariant_tsc{};
};
struct ExecutionDomain {
  CpuList cpus{};
  std::vector<Snapshot> sources{};
};
/** @brief Snapshot the current thread's allowed CPUs; never writes affinity. */
[[nodiscard]] std::expected<CpuList, AuditError> affinity_cpus() noexcept;
/** @brief Read every allowed CPU and reject changes to the allowed set; no authority is minted. */
[[nodiscard]] std::expected<ExecutionDomain, AuditError> execution_domain() noexcept;
/** @brief Cold observation; missing files/permissions remain Unknown rather than fabricated evidence. */
struct HostHardwareAudit {
  [[nodiscard]] static HostReport probe() noexcept;
};
namespace detail {
/** @brief Pure parsing seams used by the real OS collector and deterministic malformed-input tests. */
[[nodiscard]] std::expected<CpuList, AuditError> parse_cpu_list(std::string_view text) noexcept;
[[nodiscard]] std::expected<CpuList, AuditError> parse_affinity_mask(std::span<const unsigned long> mask) noexcept;
[[nodiscard]] std::expected<HugePageConfig, AuditError> parse_huge_pages(std::string_view text) noexcept;

/** @brief Testable observation algorithm; its result is data, never a permission object. */
template <class Backend>
[[nodiscard]] std::expected<ExecutionDomain, AuditError> audit_execution_domain(Backend& backend) noexcept {
  auto before = backend.affinity_cpus();
  if (!before || before->empty()) return std::unexpected(AuditError::Unavailable);
  if (before->back() > 65535 || !std::ranges::is_sorted(*before) || std::ranges::adjacent_find(*before) != before->end())
    return std::unexpected(AuditError::Malformed);
  ExecutionDomain result{*before, {}};
  result.sources.reserve(before->size());
  std::optional<AuditError> failure;
  if (before->size() == 1) {
    auto source = backend.local_snapshot();
    if (source)
      result.sources.push_back(*source);
    else
      failure = source.error();
  } else {
    for (auto cpu : *before) {
      auto source = backend.remote_snapshot(cpu);
      if (!source) {
        failure = source.error();
        break;
      }
      result.sources.push_back(*source);
    }
    if (!failure) {
      bool can_xgetbv = true;
      for (const auto& source : result.sources)
        can_xgetbv &= available(source, 1) && (source.signature.ecx & (3U << 26)) == (3U << 26);
      // A migrating thread may execute XGETBV only after every possible CPU reports the instruction's prerequisites.
      auto state = can_xgetbv ? backend.thread_xcr0() : std::optional<std::uint64_t>{};
      for (auto& source : result.sources) source.xcr0_value = state;
    }
  }
  auto after = backend.affinity_cpus();
  if (!after || *after != *before) return std::unexpected(AuditError::Unavailable);
  if (failure) return std::unexpected(*failure);
  if (result.sources.size() != result.cpus.size()) return std::unexpected(AuditError::Unavailable);
  return result;
}
}  // namespace detail
}  // namespace uarch::detect
