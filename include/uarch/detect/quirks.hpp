/** @file @brief Conservative instruction-cost and wide-vector policy observations. */
#pragma once
#include <uarch/detect/cpuid.hpp>
namespace uarch::detect::quirks {
enum class PdepCost : std::uint8_t { Fast, Slow, Unknown };
template <CpuidSource S>
[[nodiscard]] constexpr PdepCost pdep_cost(const S& source) noexcept {
  if (!available(source, 1)) return PdepCost::Unknown;
  auto bytes = vendor(source);
  std::string_view name{bytes.data(), bytes.size()};
  const auto family = family_model(source.cpuid(1, 0).eax).first;
  if ((name == "AuthenticAMD" && (family == 0x15 || family == 0x17)) || (name == "HygonGenuine" && family == 0x18))
    return PdepCost::Slow;
  if ((source.cpuid(1, 0).ecx & (1U << 31)) != 0) return PdepCost::Unknown;
  if ((name == "GenuineIntel" && family >= 6) || (name == "AuthenticAMD" && family >= 0x19)) return PdepCost::Fast;
  return PdepCost::Unknown;
}
template <CpuidSource S>
[[nodiscard]] constexpr bool avoid_wide(const S& source) noexcept {
  auto bytes = vendor(source);
  return available(source, 1) && std::string_view{bytes.data(), bytes.size()} == "GenuineIntel" &&
         family_model(source.cpuid(1, 0).eax) == std::pair<std::uint16_t, std::uint16_t>{6, 0x55};
}
}  // namespace uarch::detect::quirks
