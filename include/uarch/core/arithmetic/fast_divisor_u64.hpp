#pragma once
#include <uarch/core/arithmetic/fast_divisor_u32.hpp>

namespace uarch::arithmetic {
/// Exact libdivide-style unsigned division with precomputed add-marker parameters.
class FastDivisorU64 {
  std::uint64_t divisor_;
  std::uint64_t magic_;
  std::uint64_t more_;
  constexpr FastDivisorU64(std::uint64_t d, std::uint64_t magic, unsigned more) noexcept
      : divisor_(d), magic_(magic), more_(more) {}

 public:
  FastDivisorU64() = delete ("zero is not a valid divisor; use FastDivisorU64::try_new or divisor_u64<D>()");
  [[nodiscard]] static constexpr std::expected<FastDivisorU64, DivisorError> try_new(std::uint64_t d) noexcept {
    if (d == 0) return std::unexpected(DivisorError::Zero);
    const unsigned shift = 63U - static_cast<unsigned>(std::countl_zero(d));
    if (std::has_single_bit(d)) return FastDivisorU64(d, 0, shift);
    const __uint128_t numerator = __uint128_t{1} << (shift + 64U);
    auto magic = static_cast<std::uint64_t>(numerator / d);
    const auto remainder = static_cast<std::uint64_t>(numerator % d);
    unsigned more = shift;
    if (d - remainder >= (std::uint64_t{1} << shift)) {
      magic += magic;
      const auto twice = remainder + remainder;
      if (twice >= d || twice < remainder) ++magic;
      more |= 64U;
    }
    return FastDivisorU64(d, magic + 1, more);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t divisor() const noexcept { return divisor_; }
  [[nodiscard, gnu::always_inline]] constexpr bool is_power_of_two() const noexcept { return magic_ == 0; }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t div(std::uint64_t n) const noexcept {
    if (magic_ == 0) return n >> (more_ & 63U);
    auto q = static_cast<std::uint64_t>((static_cast<__uint128_t>(n) * magic_) >> 64);
    if ((more_ & 64U) != 0) q = ((n - q) >> 1) + q;
    return q >> (more_ & 63U);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t rem(std::uint64_t n) const noexcept {
    if (magic_ == 0) return n & (divisor_ - 1);
    return div_rem(n).second;
  }
  [[nodiscard, gnu::always_inline]] constexpr std::pair<std::uint64_t, std::uint64_t> div_rem(std::uint64_t n) const noexcept {
    if (magic_ == 0) return {n >> (more_ & 63U), n & (divisor_ - 1)};
    const auto q = div(n);
    return {q, n - q * divisor_};
  }
  [[nodiscard, gnu::always_inline]] constexpr bool is_multiple_of(std::uint64_t n) const noexcept { return rem(n) == 0; }
  constexpr bool operator==(const FastDivisorU64&) const noexcept = default;
};

template <std::uint64_t D>
[[nodiscard]] consteval FastDivisorU64 divisor_u64() {
  static_assert(D != 0, std::string_view("divisor must be non-zero"));
  return *FastDivisorU64::try_new(D);
}
}  // namespace uarch::arithmetic
