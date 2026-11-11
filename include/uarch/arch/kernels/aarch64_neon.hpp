#pragma once
#include <uarch/arch/kernels/scalar.hpp>
#if defined(__aarch64__)
#include <arm_neon.h>
namespace uarch::arch::kernels::aarch64_neon {
constexpr auto bit_gather(std::uint64_t v, std::uint64_t m) noexcept { return scalar::bit_gather(v, m); }
constexpr auto bit_scatter(std::uint64_t v, std::uint64_t m) noexcept { return scalar::bit_scatter(v, m); }
inline std::int64_t masked_sum_lt_i64x4(std::span<const std::int64_t, 4> k, std::span<const std::int64_t, 4> v,
                                        std::int64_t limit) noexcept {
  auto bound = vdupq_n_s64(limit);
  auto a = vandq_s64(vld1q_s64(v.data()), vreinterpretq_s64_u64(vcltq_s64(vld1q_s64(k.data()), bound)));
  auto b = vandq_s64(vld1q_s64(v.data() + 2), vreinterpretq_s64_u64(vcltq_s64(vld1q_s64(k.data() + 2), bound)));
  return vaddvq_s64(vaddq_s64(a, b));
}
inline std::optional<std::uint32_t> find_first_ge_i64x4(std::span<const std::int64_t, 4> k, std::int64_t target) noexcept {
  auto bound = vdupq_n_s64(target);
  auto a = vcgeq_s64(vld1q_s64(k.data()), bound);
  auto b = vcgeq_s64(vld1q_s64(k.data() + 2), bound);
  auto mask = (vgetq_lane_u64(a, 0) & 1) | (vgetq_lane_u64(a, 1) & 2) | (vgetq_lane_u64(b, 0) & 4) | (vgetq_lane_u64(b, 1) & 8);
  if (mask != 0) return static_cast<std::uint32_t>(std::countr_zero(mask));
  return std::nullopt;
}
}  // namespace uarch::arch::kernels::aarch64_neon
#endif
