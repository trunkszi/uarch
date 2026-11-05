/** @file @brief Closed unsigned storage and scoped-enum bit value contracts. */
#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace uarch::bits {
/** @brief Exactly the four fixed-width unsigned backing words. */
template <class T>
concept BitWordStorage = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t> ||
                         std::same_as<T, std::uint64_t>;
/** @brief Unsigned words or scoped enums with unsigned fixed-width representations; never bool. */
template <class T>
concept BitValue = BitWordStorage<T> || (std::is_scoped_enum_v<T> && BitWordStorage<std::underlying_type_t<T>>);
/** @brief Recoverable bit-algebra input failures. */
enum class BitError { kValueOverflow, kConflict, kInvalidString };

namespace detail {
template <class T>
concept InputValue = (std::integral<T> && sizeof(T) <= 8) || BitValue<T>;

/** @brief Numeric field arguments exclude character encodings, bool and unrelated enum domains. */
template <class T>
concept IntegerInput = std::integral<std::remove_cvref_t<T>> && sizeof(std::remove_cvref_t<T>) <= 8 &&
                       !std::same_as<std::remove_cvref_t<T>, bool> && !std::same_as<std::remove_cvref_t<T>, char> &&
                       !std::same_as<std::remove_cvref_t<T>, wchar_t> && !std::same_as<std::remove_cvref_t<T>, char8_t> &&
                       !std::same_as<std::remove_cvref_t<T>, char16_t> && !std::same_as<std::remove_cvref_t<T>, char32_t>;
template <class Input, class Member>
concept FieldInputFor = (std::is_enum_v<Member> && std::same_as<std::remove_cvref_t<Input>, Member>) ||
                        (std::same_as<Member, bool> && std::same_as<std::remove_cvref_t<Input>, bool>) ||
                        (BitWordStorage<Member> && IntegerInput<Input>);

template <class T>
consteval unsigned ValueWidth() noexcept {
  if constexpr (std::is_enum_v<T>)
    return std::numeric_limits<std::underlying_type_t<T>>::digits;
  else
    return std::numeric_limits<T>::digits;
}
template <class T>
constexpr std::uint64_t RawValue(T value) noexcept {
  if constexpr (std::is_enum_v<T>)
    return static_cast<std::uint64_t>(std::to_underlying(value));
  else
    return static_cast<std::uint64_t>(value);
}
template <InputValue T>
constexpr bool FitsValue(T value, unsigned width) noexcept {
  if constexpr (std::signed_integral<T>) {
    if (value < 0) return false;
  }
  return RawValue(value) <= (width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1);
}
constexpr std::uint64_t LowMask(unsigned width) noexcept {
  return width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width) - 1;
}
}  // namespace detail
}  // namespace uarch::bits
