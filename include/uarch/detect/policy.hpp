/** @file @brief Pure classification returns metadata, never execution capability tokens. */
#pragma once
#include <span>
#include <uarch/detect/quirks.hpp>
namespace uarch::detect {
enum class WidePolicy : std::uint8_t { Max256, Allow512 };
enum class UnknownPdep : std::uint8_t { Conservative, AssumeFast };
enum class ExecutionDomainPolicy : std::uint8_t { CommonSubset, RequireHomogeneous };
struct DetectPolicy {
  WidePolicy wide{WidePolicy::Max256};
  UnknownPdep unknown_pdep{UnknownPdep::Conservative};
  ExecutionDomainPolicy domain{ExecutionDomainPolicy::CommonSubset};
  constexpr bool operator==(const DetectPolicy&) const noexcept = default;
};
template <CpuidSource S>
[[nodiscard]] constexpr tiers::TierId classify(const S& source, DetectPolicy policy = {}) noexcept {
  if (!v3_supported(source)) return tiers::TierId::Scalar;
  auto cost = quirks::pdep_cost(source);
  if (cost == quirks::PdepCost::Slow || (cost == quirks::PdepCost::Unknown && policy.unknown_pdep == UnknownPdep::Conservative))
    return tiers::TierId::X86V3SlowPdep;
  if (policy.wide == WidePolicy::Allow512 && !quirks::avoid_wide(source) && v4_supported(source)) return tiers::TierId::X86V4;
  return tiers::TierId::X86V3;
}
[[nodiscard]] constexpr tiers::TierId intersect(tiers::TierId a, tiers::TierId b) noexcept {
  using enum tiers::TierId;
  if (!tiers::meta_of(a) || !tiers::meta_of(b) || a == Scalar || b == Scalar) return Scalar;
  if (a == Aarch64Neon || b == Aarch64Neon) return a == b ? Aarch64Neon : Scalar;
  if (a == X86V3SlowPdep || b == X86V3SlowPdep) return X86V3SlowPdep;
  if (a == X86V3 || b == X86V3) return X86V3;
  return X86V4;
}
template <CpuidSource S>
[[nodiscard]] constexpr tiers::TierId classify_domain(std::span<const S> sources, DetectPolicy policy = {}) noexcept {
  if (sources.empty()) return tiers::TierId::Scalar;
  auto result = classify(sources.front(), policy);
  for (const auto& source : sources) {
    if (policy.domain == ExecutionDomainPolicy::RequireHomogeneous && !same_isa(sources.front(), source))
      return tiers::TierId::Scalar;
    result = intersect(result, classify(source, policy));
  }
  return result;
}
}  // namespace uarch::detect
