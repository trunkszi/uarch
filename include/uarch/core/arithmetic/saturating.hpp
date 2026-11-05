#pragma once
#include <stdckdint.h>

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <optional>
#include <type_traits>

namespace uarch::arithmetic {
/// Standard signed/unsigned integer operands, excluding character and boolean types.
template <class T>
concept ArithmeticInteger =
    std::integral<T> && (sizeof(T) <= sizeof(std::uint64_t)) && (!std::same_as<T, bool>) && (!std::same_as<T, char>) &&
    (!std::same_as<T, wchar_t>) && (!std::same_as<T, char8_t>) && (!std::same_as<T, char16_t>) && (!std::same_as<T, char32_t>);
/// Saturates the mathematical result to the operand type's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr T saturating_add(T a, T b) noexcept {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_add(a, b);
#else
  return std::add_sat(a, b);
#endif
}
/// Saturates the mathematical result to the operand type's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr T saturating_sub(T a, T b) noexcept {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_sub(a, b);
#else
  return std::sub_sat(a, b);
#endif
}
/// Saturates the mathematical result to the operand type's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr T saturating_mul(T a, T b) noexcept {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_mul(a, b);
#else
  return std::mul_sat(a, b);
#endif
}
/// Saturates the mathematical result to the operand type's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr T saturating_div(T a, T b) noexcept {
  // Zero has no finite quotient; reject before invoking the standard precondition.
  if (b == 0) std::abort();
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_div(a, b);
#else
  return std::div_sat(a, b);
#endif
}
/// Clamps the input to the destination integer range before conversion.
template <ArithmeticInteger R, ArithmeticInteger T>
[[nodiscard]] constexpr R saturating_cast(T value) noexcept {
#if __cpp_lib_saturation_arithmetic >= 202603L
  return std::saturating_cast<R>(value);
#else
  return std::saturate_cast<R>(value);
#endif
}

/// Returns no result if the mathematical sum is outside T's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr std::optional<T> checked_add(T a, T b) noexcept {
  if consteval {
    constexpr auto low = std::numeric_limits<T>::min();
    constexpr auto high = std::numeric_limits<T>::max();
    if constexpr (std::is_signed_v<T>) {
      if ((b > 0 && a > high - b) || (b < 0 && a < low - b)) return std::nullopt;
    } else {
      if (a > high - b) return std::nullopt;
    }
    return static_cast<T>(a + b);
  } else {
    T result{};
    if (ckd_add(&result, a, b)) return std::nullopt;
    return result;
  }
}

/// Returns no result if the mathematical difference is outside T's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr std::optional<T> checked_sub(T a, T b) noexcept {
  if consteval {
    constexpr auto low = std::numeric_limits<T>::min();
    constexpr auto high = std::numeric_limits<T>::max();
    if constexpr (std::is_signed_v<T>) {
      if ((b > 0 && a < low + b) || (b < 0 && a > high + b)) return std::nullopt;
    } else {
      if (a < b) return std::nullopt;
    }
    return static_cast<T>(a - b);
  } else {
    T result{};
    if (ckd_sub(&result, a, b)) return std::nullopt;
    return result;
  }
}

/// Returns no result if the mathematical product is outside T's range.
template <ArithmeticInteger T>
[[nodiscard]] constexpr std::optional<T> checked_mul(T a, T b) noexcept {
  if consteval {
    // Twice-width products make the range proof safe even for INT64_MIN * -1.
    if constexpr (std::is_signed_v<T>) {
      const auto wide = static_cast<__int128_t>(a) * static_cast<__int128_t>(b);
      if (wide < std::numeric_limits<T>::min() || wide > std::numeric_limits<T>::max()) return std::nullopt;
      return static_cast<T>(wide);
    } else {
      const auto wide = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
      if (wide > std::numeric_limits<T>::max()) return std::nullopt;
      return static_cast<T>(wide);
    }
  } else {
    T result{};
    if (ckd_mul(&result, a, b)) return std::nullopt;
    return result;
  }
}
}  // namespace uarch::arithmetic
