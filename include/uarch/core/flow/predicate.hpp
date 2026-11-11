#pragma once
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
namespace uarch::flow {
template <class T>
concept MaskWord = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t> ||
                   std::same_as<T, std::uint64_t> || std::same_as<T, std::int8_t> || std::same_as<T, std::int16_t> ||
                   std::same_as<T, std::int32_t> || std::same_as<T, std::int64_t> || std::same_as<T, std::size_t> ||
                   std::same_as<T, std::ptrdiff_t>;
template <class T>
concept RegisterSelectable = MaskWord<T> && (sizeof(T) == 4 || sizeof(T) == 8);
template <class T>
concept RegisterWord = RegisterSelectable<T>;
class Predicate;
template <RegisterSelectable T>
constexpr T select_register(Predicate, T, T) noexcept;
template <class T>
constexpr T select(Predicate, T, T);
class Predicate {
  std::uint8_t bit_{0};

 public:
  constexpr Predicate() noexcept = default;
  constexpr explicit Predicate(bool b) noexcept : bit_(b) {}
  [[nodiscard, gnu::always_inline]] constexpr std::uint8_t bit() const noexcept { return bit_ != 0 ? 1U : 0U; }
  [[nodiscard, gnu::always_inline]] constexpr bool as_bool() const noexcept { return bit_ != 0; }
  [[nodiscard, gnu::always_inline]] constexpr std::int64_t select_sign_i64(std::int64_t v) const noexcept {
    const auto mask = std::uint64_t{0} - static_cast<std::uint64_t>(!as_bool());
    return std::bit_cast<std::int64_t>((std::bit_cast<std::uint64_t>(v) ^ mask) - mask);
  }
  template <class T>
  [[nodiscard, gnu::always_inline]] constexpr T select(T a, T b) const {
    return flow::select(*this, std::move(a), std::move(b));
  }
  template <RegisterSelectable T>
  [[nodiscard, gnu::always_inline]] constexpr T select_register(T a, T b) const noexcept {
    return flow::select_register(*this, a, b);
  }
  [[nodiscard, gnu::always_inline]] constexpr bool operator==(const Predicate& other) const noexcept {
    return as_bool() == other.as_bool();
  }
  [[nodiscard, gnu::always_inline]] constexpr auto operator<=>(const Predicate& other) const noexcept {
    return as_bool() <=> other.as_bool();
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Predicate operator!(Predicate p) noexcept {
    return Predicate(!p.as_bool());
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Predicate operator&(Predicate a, Predicate b) noexcept {
    return Predicate((a.bit() & b.bit()) != 0);
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Predicate operator|(Predicate a, Predicate b) noexcept {
    return Predicate((a.bit() | b.bit()) != 0);
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Predicate operator^(Predicate a, Predicate b) noexcept {
    return Predicate(a.as_bool() != b.as_bool());
  }
  template <MaskWord T>
  friend constexpr T operator*(Predicate p, T v) noexcept {
    using U = std::make_unsigned_t<T>;
    const auto bits = static_cast<U>(static_cast<U>(v) & static_cast<U>(U{0} - static_cast<U>(p.as_bool())));
    if constexpr (std::is_signed_v<T>)
      return std::bit_cast<T>(bits);
    else
      return bits;
  }
};
[[nodiscard, gnu::always_inline]] constexpr Predicate predicate(bool b) noexcept { return Predicate(b); }
}  // namespace uarch::flow
#include <uarch/core/flow/select.hpp>
