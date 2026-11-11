#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
namespace uarch::arch::kernels::scalar {
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t bit_gather(std::uint64_t val, std::uint64_t mask) noexcept {
  std::uint64_t result = 0;
  std::uint64_t output = 1;
  while (mask != 0) {
    auto bit = mask & (0ULL - mask);
    result |= (0ULL - static_cast<std::uint64_t>((val & bit) != 0)) & output;
    mask &= mask - 1ULL;
    output <<= 1U;
  }
  return result;
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t bit_scatter(std::uint64_t val, std::uint64_t mask) noexcept {
  std::uint64_t result = 0;
  while (mask != 0) {
    auto bit = mask & (0ULL - mask);
    result |= (0ULL - (val & 1ULL)) & bit;
    val >>= 1U;
    mask &= mask - 1ULL;
  }
  return result;
}
[[nodiscard, gnu::always_inline]] constexpr std::int64_t masked_sum_lt_i64x4(std::span<const std::int64_t, 4> keys,
                                                                             std::span<const std::int64_t, 4> vals,
                                                                             std::int64_t limit) noexcept {
  std::array<std::uint64_t, 4> acc{};
  for (std::size_t i = 0; i < 4; ++i) {
    const std::uint64_t mask = 0ULL - static_cast<std::uint64_t>(keys[i] < limit);
    acc[i] = static_cast<std::uint64_t>(vals[i]) & mask;
  }
  return std::bit_cast<std::int64_t>((acc[0] + acc[1]) + (acc[2] + acc[3]));
}
[[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> find_first_ge_i64x4(
    std::span<const std::int64_t, 4> keys, std::int64_t target) noexcept {
  unsigned mask = 0U;
  for (unsigned i = 0U; i < 4U; ++i) mask |= static_cast<unsigned>(keys[i] >= target) << i;
  if (mask != 0U) return static_cast<std::uint32_t>(std::countr_zero(mask));
  return std::nullopt;
}
}  // namespace uarch::arch::kernels::scalar
