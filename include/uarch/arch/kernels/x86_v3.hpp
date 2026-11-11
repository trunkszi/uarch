#pragma once
#include <uarch/arch/kernels/scalar.hpp>
#include <uarch/core/simd/vec.hpp>
#if defined(__x86_64__)
#include <immintrin.h>
namespace uarch::arch::kernels::x86_v3 {
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::uint64_t bit_gather(std::uint64_t v,
                                                                                      std::uint64_t m) noexcept {
  return _pext_u64(v, m);
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::uint64_t bit_scatter(std::uint64_t v,
                                                                                       std::uint64_t m) noexcept {
  return _pdep_u64(v, m);
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::int64_t masked_sum_lt_i64x4(
    std::span<const std::int64_t, 4> keys, std::span<const std::int64_t, 4> vals, std::int64_t limit) noexcept {
  auto k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys.data()));
  auto v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(vals.data()));
  auto keep = _mm256_cmpgt_epi64(_mm256_set1_epi64x(limit), k);
  auto selected = _mm256_and_si256(v, keep);
  using Pair = simd::vec<std::uint64_t, 2>;
  const auto pair =
      std::bit_cast<Pair>(_mm256_castsi256_si128(selected)) + std::bit_cast<Pair>(_mm256_extracti128_si256(selected, 1));
  return std::bit_cast<std::int64_t>(simd::reduce(pair));
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::optional<std::uint32_t> find_first_ge_i64x4(
    std::span<const std::int64_t, 4> keys, std::int64_t target) noexcept {
  auto k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys.data()));
  auto below = _mm256_cmpgt_epi64(_mm256_set1_epi64x(target), k);
  auto mask = static_cast<unsigned>(~_mm256_movemask_pd(_mm256_castsi256_pd(below))) & 15U;
  if (mask != 0) return static_cast<std::uint32_t>(std::countr_zero(mask));
  return std::nullopt;
}
}  // namespace uarch::arch::kernels::x86_v3
#endif
