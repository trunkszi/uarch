#pragma once
#include <uarch/arch/kernels/scalar.hpp>
#include <uarch/core/simd/vec.hpp>
#if defined(__x86_64__)
#include <immintrin.h>
namespace uarch::arch::kernels::x86_v4 {
[[gnu::target("arch=x86-64-v4"), gnu::always_inline]] inline std::uint64_t bit_gather(std::uint64_t v,
                                                                                      std::uint64_t m) noexcept {
  return _pext_u64(v, m);
}
[[gnu::target("arch=x86-64-v4"), gnu::always_inline]] inline std::uint64_t bit_scatter(std::uint64_t v,
                                                                                       std::uint64_t m) noexcept {
  return _pdep_u64(v, m);
}
[[gnu::target("arch=x86-64-v4"), gnu::always_inline]] inline std::int64_t masked_sum_lt_i64x4(
    std::span<const std::int64_t, 4> keys, std::span<const std::int64_t, 4> vals, std::int64_t limit) noexcept {
  auto k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys.data()));
  const __mmask8 keep = _mm256_cmpgt_epi64_mask(_mm256_set1_epi64x(limit), k);
  const auto selected = _mm256_maskz_loadu_epi64(keep, vals.data());
  using Pair = simd::vec<std::uint64_t, 2>;
  const auto pair =
      std::bit_cast<Pair>(_mm256_castsi256_si128(selected)) + std::bit_cast<Pair>(_mm256_extracti128_si256(selected, 1));
  return std::bit_cast<std::int64_t>(simd::reduce(pair));
}
[[gnu::target("arch=x86-64-v4"), gnu::always_inline]] inline std::optional<std::uint32_t> find_first_ge_i64x4(
    std::span<const std::int64_t, 4> keys, std::int64_t target) noexcept {
  auto k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys.data()));
  const auto mask = static_cast<unsigned>(_mm256_cmpge_epi64_mask(k, _mm256_set1_epi64x(target)));
  if (mask != 0) return static_cast<std::uint32_t>(std::countr_zero(mask));
  return std::nullopt;
}
}  // namespace uarch::arch::kernels::x86_v4
#endif
