/** @file @brief Typed read requirements and ordered clear/replace write updates. */
#pragma once

#include <cstdlib>
#include <expected>
#include <uarch/core/bits/bit_word.hpp>

namespace uarch::bits {
/** @brief Conjunction of read requirements; default construction matches every word. */
template <class Spec>
class BitPattern {
  static_assert(audit_spec<Spec>().empty(), reflect::freeze_string(audit_spec<Spec>()));
  std::uint64_t mask_{};
  std::uint64_t expected_{};
  constexpr BitPattern(std::uint64_t mask, std::uint64_t expected) noexcept : mask_(mask), expected_(expected) {}

 public:
  constexpr BitPattern() noexcept = default;
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t mask() const noexcept { return mask_; }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t expected() const noexcept { return expected_; }
  /** @brief Read-only match; requirements never modify a word. */
  [[nodiscard, gnu::always_inline]] constexpr bool matches(BitWord<Spec> word) const noexcept {
    return (word.raw() & mask_) == expected_;
  }
  /** @brief Reject contradictory requirements on overlapping bits. */
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPattern, BitError> try_and(BitPattern other) const noexcept {
    if (((expected_ ^ other.expected_) & mask_ & other.mask_) != 0) return std::unexpected(BitError::kConflict);
    return BitPattern{mask_ | other.mask_, expected_ | other.expected_};
  }
  /** @brief Conjoin requirements; conflicts fail in every profile and in constant evaluation. */
  [[nodiscard, gnu::always_inline]] constexpr BitPattern and_also(BitPattern other) const noexcept {
    auto result = try_and(other);
    if (!result) std::abort();
    return *result;
  }
  /** @brief Add a requirement for a set Flag. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Flag>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitPattern flag_set() const noexcept {
    constexpr auto kMask = std::uint64_t{1} << detail::Descriptor<Spec, M>().low;
    return and_also(BitPattern{kMask, kMask});
  }
  /** @brief Add a requirement for a clear Flag. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Flag>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitPattern flag_clear() const noexcept {
    constexpr auto kMask = std::uint64_t{1} << detail::Descriptor<Spec, M>().low;
    return and_also(BitPattern{kMask, 0});
  }
  /** @brief Checked field requirement, including compatibility with existing requirements. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>> &&
             (reflect::annotation_count_of<Field>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPattern, BitError> try_field_eq(Input value) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    constexpr auto kLow = detail::LowMask(kField.width);
    if (!detail::FitsValue(value, kField.width)) return std::unexpected(BitError::kValueOverflow);
    return try_and(BitPattern{kLow << kField.low, detail::RawValue(value) << kField.low});
  }
  /** @brief Add a checked field requirement; overflow/conflict fails in every profile. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>> &&
             (reflect::annotation_count_of<Field>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitPattern field_eq(Input value) const noexcept {
    auto result = try_field_eq<M>(value);
    if (!result) std::abort();
    return *result;
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPattern, BitError> try_flags_set() const noexcept {
    constexpr auto kMask = BitWord<Spec>::template flag_mask<Flags...>();
    return try_and(BitPattern{kMask, kMask});
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr BitPattern flags_set() const noexcept {
    auto result = try_flags_set<Flags...>();
    if (!result) std::abort();
    return *result;
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPattern, BitError> try_flags_clear() const noexcept {
    constexpr auto kMask = BitWord<Spec>::template flag_mask<Flags...>();
    return try_and(BitPattern{kMask, 0});
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr BitPattern flags_clear() const noexcept {
    auto result = try_flags_clear<Flags...>();
    if (!result) std::abort();
    return *result;
  }
  [[nodiscard, gnu::always_inline]] friend constexpr BitPattern operator&(BitPattern a, BitPattern b) noexcept {
    return a.and_also(b);
  }
  constexpr bool operator==(const BitPattern&) const noexcept = default;
};

/** @brief Write-only clear/replace update; default construction leaves a word unchanged. */
template <class Spec>
class BitPatch {
  static_assert(audit_spec<Spec>().empty(), reflect::freeze_string(audit_spec<Spec>()));
  std::uint64_t clear_mask_{};
  std::uint64_t value_bits_{};
  constexpr BitPatch(std::uint64_t mask, std::uint64_t value) noexcept : clear_mask_(mask), value_bits_(value) {}

 public:
  constexpr BitPatch() noexcept = default;
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t clear_mask() const noexcept { return clear_mask_; }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t value_bits() const noexcept { return value_bits_; }
  /** @brief Apply without reading requirements or exposing mutable word storage. */
  [[nodiscard, gnu::always_inline]] constexpr BitWord<Spec> apply(BitWord<Spec> word) const noexcept {
    return BitWord<Spec>::from_raw(static_cast<typename Spec::Word>((word.raw() & ~clear_mask_) | value_bits_));
  }
  /** @brief Ordered composition; the later patch wins on every overlapping bit. */
  [[nodiscard, gnu::always_inline]] constexpr BitPatch then(BitPatch other) const noexcept {
    return {clear_mask_ | other.clear_mask_, (value_bits_ & ~other.clear_mask_) | other.value_bits_};
  }
  /** @brief Parallel composition rejects conflicting writes. */
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPatch, BitError> try_and(BitPatch other) const noexcept {
    if (((value_bits_ ^ other.value_bits_) & clear_mask_ & other.clear_mask_) != 0) return std::unexpected(BitError::kConflict);
    return BitPatch{clear_mask_ | other.clear_mask_, value_bits_ | other.value_bits_};
  }
  [[nodiscard, gnu::always_inline]] constexpr BitPatch and_also(BitPatch other) const noexcept {
    auto result = try_and(other);
    if (!result) std::abort();
    return *result;
  }
  /** @brief Add a non-conflicting flag write; use then for deliberate overwrite. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Flag>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitPatch set_flag(bool enabled) const noexcept {
    constexpr auto kMask = std::uint64_t{1} << detail::Descriptor<Spec, M>().low;
    return and_also(BitPatch{kMask, enabled ? kMask : 0});
  }
  /** @brief Checked field write, rejecting truncation and conflicting existing writes. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>> &&
             (reflect::annotation_count_of<Field>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPatch, BitError> try_set_field(Input value) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    constexpr auto kLow = detail::LowMask(kField.width);
    if (!detail::FitsValue(value, kField.width)) return std::unexpected(BitError::kValueOverflow);
    return try_and(BitPatch{kLow << kField.low, detail::RawValue(value) << kField.low});
  }
  /** @brief Add a checked field write; failures abort in every runtime profile. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>> &&
             (reflect::annotation_count_of<Field>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitPatch set_field(Input value) const noexcept {
    auto result = try_set_field<M>(value);
    if (!result) std::abort();
    return *result;
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPatch, BitError> try_set_flags() const noexcept {
    constexpr auto kMask = BitWord<Spec>::template flag_mask<Flags...>();
    return try_and(BitPatch{kMask, kMask});
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr BitPatch set_flags() const noexcept {
    auto result = try_set_flags<Flags...>();
    if (!result) std::abort();
    return *result;
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitPatch, BitError> try_clear_flags() const noexcept {
    constexpr auto kMask = BitWord<Spec>::template flag_mask<Flags...>();
    return try_and(BitPatch{kMask, 0});
  }
  template <std::meta::info... Flags>
    requires(sizeof...(Flags) > 0)
  [[nodiscard, gnu::always_inline]] constexpr BitPatch clear_flags() const noexcept {
    auto result = try_clear_flags<Flags...>();
    if (!result) std::abort();
    return *result;
  }
  constexpr bool operator==(const BitPatch&) const noexcept = default;
};
}  // namespace uarch::bits
