/** @file @brief Pure CPUID evidence contracts and bounded ISA feature queries. */
#pragma once
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string_view>
#include <uarch/tiers/tier_id.hpp>
#include <utility>

namespace uarch::detect {
struct CpuidRegisters {
  std::uint32_t eax{}, ebx{}, ecx{}, edx{};
  constexpr bool operator==(const CpuidRegisters&) const noexcept = default;
};
/** @brief An observation seam; satisfying it grants no capability. */
template <class S>
concept CpuidSource = requires(const S& source, std::uint32_t leaf, std::uint32_t subleaf) {
  { source.cpuid(leaf, subleaf) } -> std::same_as<CpuidRegisters>;
  { source.xcr0() } -> std::same_as<std::optional<std::uint64_t>>;
};
struct HardwareCpuid {
  constexpr HardwareCpuid() noexcept = default;
  constexpr HardwareCpuid(const HardwareCpuid&) noexcept = default;
  constexpr HardwareCpuid(HardwareCpuid&&) noexcept = default;
  constexpr HardwareCpuid& operator=(const HardwareCpuid& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr HardwareCpuid& operator=(HardwareCpuid&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~HardwareCpuid() noexcept = default;
  constexpr bool operator==(const HardwareCpuid&) const noexcept = default;
  [[nodiscard]] CpuidRegisters cpuid(std::uint32_t leaf, std::uint32_t subleaf) const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> xcr0() const noexcept;
};
namespace detail {
struct Requirement {
  tiers::Feature id;
  std::string_view name;
  std::uint32_t leaf;
  bool ebx;
  unsigned bit;
};
inline constexpr std::array kRequirements{Requirement{tiers::Feature::kAvx, "avx", 1, false, 28},
                                          Requirement{tiers::Feature::kAvx2, "avx2", 7, true, 5},
                                          Requirement{tiers::Feature::kBmi1, "bmi1", 7, true, 3},
                                          Requirement{tiers::Feature::kBmi2, "bmi2", 7, true, 8},
                                          Requirement{tiers::Feature::kCmpxchg16b, "cmpxchg16b", 1, false, 13},
                                          Requirement{tiers::Feature::kF16c, "f16c", 1, false, 29},
                                          Requirement{tiers::Feature::kFma, "fma", 1, false, 12},
                                          Requirement{tiers::Feature::kLzcnt, "lzcnt", 0x80000001, false, 5},
                                          Requirement{tiers::Feature::kLahfLm, "lahf_lm", 0x80000001, false, 0},
                                          Requirement{tiers::Feature::kMovbe, "movbe", 1, false, 22},
                                          Requirement{tiers::Feature::kPopcnt, "popcnt", 1, false, 23},
                                          Requirement{tiers::Feature::kSse3, "sse3", 1, false, 0},
                                          Requirement{tiers::Feature::kSsse3, "ssse3", 1, false, 9},
                                          Requirement{tiers::Feature::kSse41, "sse4.1", 1, false, 19},
                                          Requirement{tiers::Feature::kSse42, "sse4.2", 1, false, 20},
                                          Requirement{tiers::Feature::kXsave, "xsave", 1, false, 26},
                                          Requirement{tiers::Feature::kAvx512f, "avx512f", 7, true, 16},
                                          Requirement{tiers::Feature::kAvx512bw, "avx512bw", 7, true, 30},
                                          Requirement{tiers::Feature::kAvx512cd, "avx512cd", 7, true, 28},
                                          Requirement{tiers::Feature::kAvx512dq, "avx512dq", 7, true, 17},
                                          Requirement{tiers::Feature::kAvx512vl, "avx512vl", 7, true, 31}};
static_assert([] {
  std::uint32_t bits = 0;
  for (auto r : kRequirements) {
    if (bits & std::to_underlying(r.id)) return false;
    bits |= std::to_underlying(r.id);
  }
  return bits == tiers::kX86V4Features;
}());
}  // namespace detail
template <CpuidSource S>
[[nodiscard]] constexpr bool available(const S& source, std::uint32_t leaf) noexcept {
  return source.cpuid(leaf >= 0x80000000 ? 0x80000000 : 0, 0).eax >= leaf;
}
template <CpuidSource S>
[[nodiscard]] constexpr bool feature(const S& source, std::string_view name) noexcept {
  for (auto r : detail::kRequirements)
    if (r.name == name) {
      if (!available(source, r.leaf)) return false;
      auto registers = source.cpuid(r.leaf, 0);
      return ((r.ebx ? registers.ebx : registers.ecx) & (1U << r.bit)) != 0;
    }
  return false;
}
template <CpuidSource S>
[[nodiscard]] constexpr bool os_state(const S& source, std::uint64_t mask) noexcept {
  if (!available(source, 1) || (source.cpuid(1, 0).ecx & (3U << 26)) != (3U << 26)) return false;
  auto state = source.xcr0();
  return state && (*state & mask) == mask;
}
template <CpuidSource S>
[[nodiscard]] constexpr bool v3_supported(const S& source) noexcept {
  if (!os_state(source, 0x6)) return false;
  return std::ranges::all_of(detail::kRequirements, [&](const auto& r) {
    return (std::to_underlying(r.id) & tiers::kX86V3Features) == 0 || feature(source, r.name);
  });
}
template <CpuidSource S>
[[nodiscard]] constexpr bool v4_supported(const S& source) noexcept {
  if (!os_state(source, 0xe6)) return false;
  return std::ranges::all_of(detail::kRequirements, [&](const auto& r) { return feature(source, r.name); });
}
template <CpuidSource S>
[[nodiscard]] constexpr bool prefetch_write_supported(const S& source) noexcept {
  return available(source, 0x80000001) && (source.cpuid(0x80000001, 0).ecx & (1U << 8)) != 0;
}
template <CpuidSource A, CpuidSource B>
[[nodiscard]] constexpr bool same_isa(const A& a, const B& b) noexcept {
  for (auto r : detail::kRequirements)
    if (feature(a, r.name) != feature(b, r.name)) return false;
  auto os_flags = []<CpuidSource S>(const S& s) { return available(s, 1) ? s.cpuid(1, 0).ecx & (3U << 26) : 0U; };
  return os_flags(a) == os_flags(b) && a.xcr0() == b.xcr0() && prefetch_write_supported(a) == prefetch_write_supported(b);
}
[[nodiscard]] constexpr std::pair<std::uint16_t, std::uint16_t> family_model(std::uint32_t signature) noexcept {
  auto base = (signature >> 8) & 15U;
  return {static_cast<std::uint16_t>(base + (base == 15 ? ((signature >> 20) & 255U) : 0)),
          static_cast<std::uint16_t>(((signature >> 4) & 15U) + ((base == 6 || base == 15) ? ((signature >> 12) & 240U) : 0))};
}
template <CpuidSource S>
[[nodiscard]] constexpr std::array<char, 12> vendor(const S& source) noexcept {
  const auto r = source.cpuid(0, 0);
  std::array<char, 12> result{};
  for (unsigned i = 0; i < 4; ++i) {
    result[i] = static_cast<char>((r.ebx >> (8 * i)) & 255U);
    result[i + 4] = static_cast<char>((r.edx >> (8 * i)) & 255U);
    result[i + 8] = static_cast<char>((r.ecx >> (8 * i)) & 255U);
  }
  return result;
}
/** @brief A frozen observation, not permission to execute an instruction. */
struct Snapshot {
  CpuidRegisters basic{}, signature{}, structured{}, extended{}, extended_features{}, invariant{};
  std::optional<std::uint64_t> xcr0_value;
  [[nodiscard]] constexpr CpuidRegisters cpuid(std::uint32_t leaf, std::uint32_t subleaf) const noexcept {
    if (subleaf != 0) return {};
    switch (leaf) {
      case 0:
        return basic;
      case 1:
        return signature;
      case 7:
        return structured;
      case 0x80000000:
        return extended;
      case 0x80000001:
        return extended_features;
      case 0x80000007:
        return invariant;
      default:
        return {};
    }
  }
  [[nodiscard]] constexpr std::optional<std::uint64_t> xcr0() const noexcept { return xcr0_value; }
  template <CpuidSource S>
  [[nodiscard]] static constexpr Snapshot from_source(const S& source) noexcept {
    Snapshot result;
    result.basic = source.cpuid(0, 0);
    result.extended = source.cpuid(0x80000000, 0);
    if (result.basic.eax >= 1) result.signature = source.cpuid(1, 0);
    if (result.basic.eax >= 7) result.structured = source.cpuid(7, 0);
    if (result.extended.eax >= 0x80000001) result.extended_features = source.cpuid(0x80000001, 0);
    if (result.extended.eax >= 0x80000007) result.invariant = source.cpuid(0x80000007, 0);
    result.xcr0_value = source.xcr0();
    return result;
  }
  constexpr bool operator==(const Snapshot&) const noexcept = default;
};
static_assert(CpuidSource<Snapshot> && CpuidSource<HardwareCpuid>);
}  // namespace uarch::detect
