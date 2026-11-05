#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
#include <uarch/core/arithmetic/power_of_two.hpp>
#include <uarch/core/arithmetic/saturating.hpp>
extern "C" [[gnu::noinline]] std::uint64_t fast_divisor_u64_div(uarch::arithmetic::FastDivisorU64 divisor, std::uint64_t n) {
  return divisor.div(n);
}
extern "C" [[gnu::noinline]] std::uint32_t fast_divisor_u32_rem(uarch::arithmetic::FastDivisorU32 divisor, std::uint32_t n) {
  return divisor.rem(n);
}

extern "C" [[gnu::noinline]] std::uint64_t fast_divisor_u64_rem(uarch::arithmetic::FastDivisorU64 divisor, std::uint64_t n) {
  return divisor.rem(n);
}
extern "C" [[gnu::noinline]] std::uint64_t fast_divisor_u64_pow2_rem(uarch::arithmetic::FastDivisorU64 divisor,
                                                                     std::uint64_t n) {
  if (!divisor.is_power_of_two()) __builtin_unreachable();
  return divisor.rem(n);
}
extern "C" [[gnu::noinline]] std::uint64_t power_of_two_align(std::uint64_t n) {
  using Boundary = uarch::arithmetic::PowerOfTwo<6>;
  return Boundary::align_down(n) ^ Boundary::alignment_offset(n);
}
extern "C" [[gnu::noinline]] std::uint64_t saturating_add_u64(std::uint64_t a, std::uint64_t b) {
  return uarch::arithmetic::saturating_add(a, b);
}
