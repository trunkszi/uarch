#pragma once
#include <uarch/arch/kernels/x86_v3.hpp>
#if defined(__x86_64__)
namespace uarch::arch::kernels::x86_v3_slow_pdep {
// The assembly boundary prevents BMI2-enabled optimizers from synthesizing microcoded PEXT/PDEP.
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::uint64_t bit_gather(std::uint64_t val,
                                                                                      std::uint64_t mask) noexcept {
  std::uint64_t result = 0;
  std::uint64_t output = 1;
  while (mask != 0) {
    std::uint64_t index = 0;
    std::uint64_t next = 0;
    asm("tzcnt %[mask], %[index]\n\tblsr %[mask], %[next]" : [index] "=&r"(index), [next] "=r"(next) : [mask] "r"(mask) : "cc");
    result |= (0ULL - ((val >> index) & 1ULL)) & output;
    output <<= 1U;
    mask = next;
  }
  return result;
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::uint64_t bit_scatter(std::uint64_t val,
                                                                                       std::uint64_t mask) noexcept {
  std::uint64_t result = 0;
  while (mask != 0) {
    std::uint64_t index = 0;
    std::uint64_t next = 0;
    asm("tzcnt %[mask], %[index]\n\tblsr %[mask], %[next]" : [index] "=&r"(index), [next] "=r"(next) : [mask] "r"(mask) : "cc");
    result |= (val & 1ULL) << index;
    val >>= 1U;
    mask = next;
  }
  return result;
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::int64_t masked_sum_lt_i64x4(
    std::span<const std::int64_t, 4> k, std::span<const std::int64_t, 4> v, std::int64_t limit) noexcept {
  return x86_v3::masked_sum_lt_i64x4(k, v, limit);
}
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::optional<std::uint32_t> find_first_ge_i64x4(
    std::span<const std::int64_t, 4> k, std::int64_t target) noexcept {
  return x86_v3::find_first_ge_i64x4(k, target);
}
}  // namespace uarch::arch::kernels::x86_v3_slow_pdep
#endif
