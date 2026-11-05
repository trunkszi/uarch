/** @file @brief Five descriptive ISA tiers and structural reflection-generated metadata. */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <uarch/core/reflect.hpp>
#include <utility>

namespace uarch::tiers {
/** @brief Feature bits mirror the complete Rust tier requirements, independent of compiler spelling. */
enum class Feature : std::uint32_t {
  kAvx = 1U << 0,
  kAvx2 = 1U << 1,
  kBmi1 = 1U << 2,
  kBmi2 = 1U << 3,
  kCmpxchg16b = 1U << 4,
  kF16c = 1U << 5,
  kFma = 1U << 6,
  kLzcnt = 1U << 7,
  kLahfLm = 1U << 21,
  kMovbe = 1U << 8,
  kPopcnt = 1U << 9,
  kSse3 = 1U << 10,
  kSsse3 = 1U << 11,
  kSse41 = 1U << 12,
  kSse42 = 1U << 13,
  kXsave = 1U << 14,
  kAvx512f = 1U << 15,
  kAvx512bw = 1U << 16,
  kAvx512cd = 1U << 17,
  kAvx512dq = 1U << 18,
  kAvx512vl = 1U << 19,
  kNeon = 1U << 20
};
inline constexpr std::uint32_t kX86V3Features = ((1U << 15) - 1) | std::to_underlying(Feature::kLahfLm);
inline constexpr std::uint32_t kX86V4Features = ((1U << 20) - 1) | std::to_underlying(Feature::kLahfLm);
inline constexpr std::uint32_t kNeonFeatures = std::to_underlying(Feature::kNeon);

/** @brief Structural annotation owning its name; no pointer into a temporary literal payload. */
struct TierMeta {
  char name[32]{};                 /**< Null-terminated stable tier spelling. */
  std::size_t name_len{};          /**< Logical name length, excluding the terminator. */
  std::uint32_t vector_bytes{};    /**< Preferred vector width in bytes. */
  std::uint32_t max_vector_bits{}; /**< Maximum tier vector width in bits. */
  bool fast_pdep{};                /**< Whether hardware PDEP/PEXT is preferred. */
  std::uint32_t feature_flags{};   /**< Complete required ISA feature set. */

  char target_cpu[16]{};         /**< Compiler CPU model spelling, independent of runtime detection. */
  std::size_t target_cpu_len{};  /**< CPU spelling length without its terminator. */
  char target_arch[16]{};        /**< Target architecture spelling, or generic for Scalar. */
  std::size_t target_arch_len{}; /**< Architecture spelling length without its terminator. */

  template <std::size_t N, std::size_t CpuN = 8, std::size_t ArchN = 8>
  consteval TierMeta(const char (&text)[N], std::uint32_t bytes, std::uint32_t bits, bool fast, std::uint32_t features,
                     const char (&cpu)[CpuN] = "generic", const char (&arch)[ArchN] = "generic") noexcept
      : name_len(N - 1),
        vector_bytes(bytes),
        max_vector_bits(bits),
        fast_pdep(fast),
        feature_flags(features),
        target_cpu_len(CpuN - 1),
        target_arch_len(ArchN - 1) {
    static_assert(N <= 32, std::string_view("tier name exceeds structural buffer"));
    static_assert(CpuN <= 16 && ArchN <= 16, std::string_view("tier target exceeds structural buffer"));
    for (std::size_t i = 0; i < N; ++i) name[i] = text[i];
    for (std::size_t i = 0; i < CpuN; ++i) target_cpu[i] = cpu[i];
    for (std::size_t i = 0; i < ArchN; ++i) target_arch[i] = arch[i];
  }
  constexpr TierMeta() noexcept = default;
  constexpr bool operator==(const TierMeta&) const noexcept = default;
};

/** @brief Stable metadata IDs, never evidence that the host can execute an ISA. */
enum class TierId : std::uint8_t {
  Scalar[[= TierMeta{"scalar", 0, 64, false, 0, "generic", "generic"}]] = 0,
  X86V3[[= TierMeta{"x86_v3", 32, 256, true, kX86V3Features, "x86-64-v3", "x86_64"}]] = 1,
  X86V3SlowPdep[[= TierMeta{"x86_v3_slow_pdep", 32, 256, false, kX86V3Features, "x86-64-v3", "x86_64"}]] = 2,
  X86V4[[= TierMeta{"x86_v4", 64, 512, true, kX86V4Features, "x86-64-v4", "x86_64"}]] = 3,
  Aarch64Neon[[= TierMeta{"aarch64_neon", 16, 128, false, kNeonFeatures, "generic", "aarch64"}]] = 4
};

namespace detail {
/** @brief Validate exactly one metadata payload per dense enum value before freezing its table. */
template <class Enum>
consteval auto MakeMetadata() {
  constexpr auto kEnumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));
  std::array<TierMeta, kEnumerators.size()> result{};
  std::size_t index = 0;
  template for (constexpr auto kEnumerator : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    static_assert(reflect::annotation_count_of<TierMeta>(kEnumerator) == 1,
                  std::string_view("each tier requires exactly one TierMeta annotation"));
    constexpr auto kNote = reflect::annotation_of<TierMeta>(kEnumerator);
    if constexpr (kNote.has_value()) {
      if (static_cast<std::size_t>(std::to_underlying([:kEnumerator:])) != index) std::abort();
      result[index] = *kNote;
    }
    ++index;
  }
  return result;
}

template <class Enum>
consteval auto MakeIds() {
  constexpr auto kEnumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));
  std::array<Enum, kEnumerators.size()> result{};
  std::size_t index = 0;
  template for (constexpr auto kEnumerator : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    result[index++] = [:kEnumerator:];
  }
  return result;
}
inline constexpr auto kMetadata = MakeMetadata<TierId>();
inline constexpr auto kTierIds = MakeIds<TierId>();
static_assert(kMetadata.size() == 5);

}  // namespace detail

/** @brief Copy a tier's metadata, or reject an out-of-range integer enum value. */
[[nodiscard]] constexpr std::optional<TierMeta> meta_of(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  return detail::kMetadata[index];
}
/** @brief Static-storage name view; invalid IDs have no name. */
[[nodiscard]] constexpr std::optional<std::string_view> as_str(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  const auto& meta = detail::kMetadata[index];
  return std::string_view{meta.name, meta.name_len};
}
/** @brief Maximum vector width for a valid tier. */
[[nodiscard]] constexpr std::optional<std::uint32_t> max_vector_bits(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  return detail::kMetadata[index].max_vector_bits;
}
/** @brief Preferred vector width in bytes for a valid tier. */
[[nodiscard]] constexpr std::optional<std::uint32_t> vector_bytes(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  return detail::kMetadata[index].vector_bytes;
}
/** @brief PDEP/PEXT cost policy, separate from mere instruction availability. */
[[nodiscard]] constexpr std::optional<bool> has_fast_pdep(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  return detail::kMetadata[index].fast_pdep;
}
/** @brief Compiler CPU model for a valid descriptive tier. */
[[nodiscard]] constexpr std::optional<std::string_view> target_cpu(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  const auto& meta = detail::kMetadata[index];
  return std::string_view{meta.target_cpu, meta.target_cpu_len};
}
/** @brief Architecture spelling for a valid descriptive tier. */
[[nodiscard]] constexpr std::optional<std::string_view> target_arch(TierId tier) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(tier));
  if (index >= detail::kMetadata.size()) return std::nullopt;
  const auto& meta = detail::kMetadata[index];
  return std::string_view{meta.target_arch, meta.target_arch_len};
}
/** @brief All five stable IDs, in numeric order. */
[[nodiscard]] constexpr std::span<const TierId, 5> all_tiers() noexcept { return detail::kTierIds; }
/** @brief Parse a bounded name against five fixed names; no allocation or unbounded input scan. */
[[nodiscard]] constexpr std::optional<TierId> from_str(std::string_view text) noexcept {
  if (text.size() >= 32) return std::nullopt;
  for (auto tier : all_tiers()) {
    if (as_str(tier) == text) return tier;
  }
  return std::nullopt;
}
}  // namespace uarch::tiers
