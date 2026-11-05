#pragma once
#include <bit>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include <utility>

namespace uarch::arithmetic {
/// A zero divisor is rejected before reciprocal generation.
enum class DivisorError : std::uint8_t { Zero };

/// Exact unsigned division and Lemire remainder with a frozen nonzero divisor.
class FastDivisorU32 {
  std::uint64_t magic_{0};
  std::uint32_t divisor_;
  std::uint32_t reserved_{0};
  constexpr explicit FastDivisorU32(std::uint32_t d) noexcept
      : magic_(std::numeric_limits<std::uint64_t>::max() / d + 1), divisor_(d) {}

 public:
  FastDivisorU32() = delete ("zero is not a valid divisor; use FastDivisorU32::try_new or divisor_u32<D>()");
  [[nodiscard]] static constexpr std::expected<FastDivisorU32, DivisorError> try_new(std::uint32_t d) noexcept {
    if (d == 0) return std::unexpected(DivisorError::Zero);
    return FastDivisorU32(d);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint32_t divisor() const noexcept { return divisor_; }
  [[nodiscard]] constexpr bool is_power_of_two() const noexcept { return std::has_single_bit(divisor_); }
  [[nodiscard, gnu::always_inline]] constexpr std::uint32_t div(std::uint32_t n) const noexcept {
    // d=1 wraps the reciprocal to zero and needs its exact identity quotient.
    return magic_ == 0 ? n : static_cast<std::uint32_t>((static_cast<__uint128_t>(magic_) * n) >> 64);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint32_t rem(std::uint32_t n) const noexcept {
    const std::uint64_t low = magic_ * n;
    return static_cast<std::uint32_t>((static_cast<__uint128_t>(low) * divisor_) >> 64);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::pair<std::uint32_t, std::uint32_t> div_rem(std::uint32_t n) const noexcept {
    const auto q = div(n);
    return {q, n - q * divisor_};
  }
  [[nodiscard]] constexpr bool is_multiple_of(std::uint32_t n) const noexcept { return n * magic_ <= magic_ - 1; }
  constexpr bool operator==(const FastDivisorU32&) const noexcept = default;
};

template <std::uint32_t D>
[[nodiscard]] consteval FastDivisorU32 divisor_u32() {
  static_assert(D != 0, std::string_view("divisor must be non-zero"));
  return *FastDivisorU32::try_new(D);
}
}  // namespace uarch::arithmetic
