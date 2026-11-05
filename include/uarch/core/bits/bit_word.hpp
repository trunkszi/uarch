/** @file @brief Reflection-audited word storage and typed member operations. */
#pragma once

#include <bitset>
#include <compare>
#include <cstdlib>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>
#include <uarch/core/bits/annotations.hpp>
#include <uarch/core/bits/bit_value.hpp>
#include <uarch/core/reflect.hpp>

namespace uarch::bits {
/** @brief Diagnose the first invalid annotated member; empty text denotes a valid specification. */
template <class Spec>
[[nodiscard]] consteval std::string audit_spec() {
  if constexpr (!requires { typename Spec::Word; })
    return "specification requires Word";
  else if constexpr (!BitWordStorage<typename Spec::Word>)
    return "invalid backing Word";
  else if constexpr (!std::is_class_v<Spec> || std::is_union_v<Spec>)
    return "specification must be a record";
  else {
    if (!std::meta::bases_of(^^Spec, reflect::unchecked_context()).empty()) return "specification bases are unsupported";
    std::uint64_t used = 0;
    std::string error;
    template for (constexpr auto kMember :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^Spec, reflect::unchecked_context()))) {
      if (error.empty()) {
        const std::string name{std::meta::identifier_of(kMember)};
        if constexpr (std::meta::is_bit_field(kMember))
          error = "C++ bitfield: " + name;
        else {
          constexpr auto kFlags = reflect::annotation_count_of<Flag>(kMember);
          constexpr auto kFields = reflect::annotation_count_of<Field>(kMember);
          constexpr auto kNotes = std::define_static_array(std::meta::annotations_of(kMember));
          if constexpr (kFlags + kFields != 1 || kNotes.size() != 1)
            error = "exactly one Flag or Field required: " + name;
          else {
            using Value = [:std::meta::type_of(kMember):];
            if constexpr (!(BitValue<Value> || (kFlags == 1 && std::same_as<Value, bool>)))
              error = "invalid bit value type: " + name;
            else {
              constexpr auto kDescriptor = []() consteval {
                if constexpr (kFlags == 1)
                  return Field{reflect::annotation_of<Flag>(kMember)->bit, 1};
                else
                  return *reflect::annotation_of<Field>(kMember);
              }();
              constexpr unsigned kBits = sizeof(typename Spec::Word) * 8;
              if constexpr (kDescriptor.width == 0 || kDescriptor.low >= kBits || kDescriptor.width > kBits - kDescriptor.low)
                error = "outside word bounds: " + name;
              else if constexpr (kDescriptor.width > detail::ValueWidth<Value>())
                error = "value width overflow: " + name;
              else {
                constexpr auto kMask = detail::LowMask(kDescriptor.width) << kDescriptor.low;
                if ((used & kMask) != 0) error = "overlapping field: " + name;
                used |= kMask;
              }
            }
          }
        }
      }
    }
    return error;
  }
}

namespace detail {
template <class Spec, std::meta::info Member>
concept OwnMember = std::meta::is_nonstatic_data_member(Member) && std::meta::parent_of(Member) == ^^Spec;

template <class Spec, std::meta::info Member>
  requires OwnMember<Spec, Member>
consteval Field Descriptor() {
  if constexpr (reflect::annotation_count_of<Flag>(Member) == 1)
    return {reflect::annotation_of<Flag>(Member)->bit, 1};
  else
    return *reflect::annotation_of<Field>(Member);
}
template <std::meta::info Member>
using MemberValue = [:std::meta::type_of(Member):];
}  // namespace detail

/** @brief One unsigned backing word whose field descriptors belong to its specification. */
template <class Spec>
class BitWord {
  static_assert(audit_spec<Spec>().empty(), reflect::freeze_string(audit_spec<Spec>()));

 public:
  using Word = typename Spec::Word;

 private:
  Word raw_{};

 public:
  /** @brief Zero-initialized storage. */
  constexpr BitWord() noexcept = default;
  /** @brief Convert a bitset by value operations, without depending on its object representation. */
  explicit constexpr BitWord(std::bitset<sizeof(Word) * 8> bits) noexcept : raw_(static_cast<Word>(bits.to_ullong())) {}
  /** @brief Every backing-word bit pattern is valid storage. */
  [[nodiscard, gnu::always_inline]] static constexpr BitWord from_raw(Word raw) noexcept {
    BitWord result;
    result.raw_ = raw;
    return result;
  }
  /** @brief Original backing-word bits. */
  [[nodiscard, gnu::always_inline]] constexpr Word raw() const noexcept { return raw_; }
  /** @brief Standard bitset interoperability. */
  [[nodiscard]] constexpr auto to_bitset() const noexcept { return std::bitset<sizeof(Word) * 8>{raw_}; }
  constexpr auto operator<=>(const BitWord&) const noexcept = default;

  /** @brief Extract an unsigned/scoped-enum field or boolean flag. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M>
  [[nodiscard, gnu::always_inline]] constexpr auto get() const noexcept -> detail::MemberValue<M> {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    return static_cast<detail::MemberValue<M>>((static_cast<std::uint64_t>(raw_) >> kField.low) &
                                               detail::LowMask(kField.width));
  }
  /** @brief Test a Flag member, including an unsigned single-bit value. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Flag>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr bool test() const noexcept {
    return detail::RawValue(get<M>()) != 0;
  }
  /** @brief Replace a field, deliberately truncating values modulo its bit width. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>>
  [[nodiscard, gnu::always_inline]] constexpr BitWord with(Input value) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    constexpr auto kLow = detail::LowMask(kField.width);
    constexpr auto kMask = kLow << kField.low;
    return from_raw(
        static_cast<Word>((static_cast<std::uint64_t>(raw_) & ~kMask) | ((detail::RawValue(value) & kLow) << kField.low)));
  }
  /** @brief Reject a constant value that would be truncated, including negative constants. */
  template <std::meta::info M, auto Value>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<decltype(Value), detail::MemberValue<M>>
  [[nodiscard, gnu::always_inline]] constexpr BitWord with_const() const noexcept {
    static_assert(std::is_integral_v<decltype(Value)> || BitValue<decltype(Value)>);
    constexpr auto kField = detail::Descriptor<Spec, M>();
    if constexpr (std::is_signed_v<decltype(Value)>) static_assert(Value >= 0, "field value overflow");
    static_assert(detail::RawValue(Value) <= detail::LowMask(kField.width), "field value overflow");
    return with<M>(static_cast<detail::MemberValue<M>>(Value));
  }
  /** @brief Checked replacement, preserving this word on overflow. */
  template <std::meta::info M, class Input>
    requires detail::OwnMember<Spec, M> && detail::FieldInputFor<Input, detail::MemberValue<M>>
  [[nodiscard, gnu::always_inline]] constexpr std::expected<BitWord, BitError> try_with(Input value) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    if (!detail::FitsValue(value, kField.width)) return std::unexpected(BitError::kValueOverflow);
    return with<M>(value);
  }
  /** @brief Add modulo the field width while preserving every neighboring bit. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Field>(M) == 1)
  [[nodiscard, gnu::always_inline]] constexpr BitWord wrapping_add_field(std::uint64_t delta) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    const auto next = (detail::RawValue(get<M>()) + delta) & detail::LowMask(kField.width);
    return with<M>(static_cast<detail::MemberValue<M>>(next));
  }
  /** @brief Add directly to a topmost field; the backing-word conversion supplies wrapping. */
  template <std::meta::info M>
    requires detail::OwnMember<Spec, M> && (reflect::annotation_count_of<Field>(M) == 1) &&
             (detail::Descriptor<Spec, M>().low + detail::Descriptor<Spec, M>().width == sizeof(Word) * 8)
  [[nodiscard, gnu::always_inline]] constexpr BitWord wrapping_add_top_field(std::uint64_t delta) const noexcept {
    constexpr auto kField = detail::Descriptor<Spec, M>();
    return from_raw(static_cast<Word>(static_cast<std::uint64_t>(raw_) + (delta << kField.low)));
  }
};
}  // namespace uarch::bits
