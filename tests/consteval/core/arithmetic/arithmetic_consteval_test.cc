#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
#include <uarch/core/arithmetic/power_of_two.hpp>
#include <uarch/core/arithmetic/saturating.hpp>
#include <uarch/core/mem/prefetch.hpp>
using namespace uarch::arithmetic;
consteval bool powers() {
  for (unsigned bit = 0; bit < 64; ++bit) {
    const auto d = std::uint64_t{1} << bit;
    const auto divisor = FastDivisorU64::try_new(d);
    if (!divisor || divisor->div(UINT64_MAX) != UINT64_MAX / d || divisor->rem(UINT64_MAX) != UINT64_MAX % d) return false;
    if (bit < 32) {
      const auto small_d = static_cast<std::uint32_t>(d);
      const auto small = FastDivisorU32::try_new(small_d);
      if (!small || small->div(UINT32_MAX) != UINT32_MAX / small_d || small->rem(UINT32_MAX) != UINT32_MAX % small_d)
        return false;
    }
  }
  return true;
}
static_assert(powers());
static_assert(divisor_u32<1>().div(UINT32_MAX) == UINT32_MAX);
static_assert(divisor_u32<7>().rem(100) == 2);
static_assert(divisor_u64<7>().div_rem(100) == std::pair<std::uint64_t, std::uint64_t>{14, 2});
static_assert(PowerOfTwo<0>::checked_align_up(UINT64_MAX) == UINT64_MAX);
static_assert(!PowerOfTwo<63>::checked_align_up(UINT64_MAX));
static_assert(PowerOfTwo<63>::alignment_offset(1) == (std::uint64_t{1} << 63) - 1);
static_assert(checked_add(1, 2) == 3 && !checked_add(INT64_MAX, std::int64_t{1}));
static_assert(checked_sub(2U, 1U) == 1U && !checked_sub(0U, 1U));
static_assert(!checked_sub(INT64_MIN, std::int64_t{1}));
static_assert(!checked_add(INT64_MIN, std::int64_t{-1}));
static_assert(!checked_mul(INT64_MIN, std::int64_t{-1}));
static_assert(!checked_mul(UINT64_MAX, std::uint64_t{2}));
static_assert(checked_mul(std::int64_t{-3}, std::int64_t{-7}) == 21);
static_assert(checked_add(INT64_MIN, INT64_MAX) == -1);
static_assert(checked_sub(INT64_MIN, INT64_MIN) == 0);
static_assert(checked_sub(std::int64_t{0}, INT64_MIN) == std::nullopt);
static_assert(checked_add(UINT64_MAX, std::uint64_t{0}) == UINT64_MAX);
static_assert(!checked_add(UINT64_MAX, std::uint64_t{1}));
static_assert(checked_mul(std::uint64_t{0}, UINT64_MAX) == 0);
static_assert(checked_mul(INT64_MIN, std::int64_t{1}) == INT64_MIN);
static_assert(saturating_add(UINT64_MAX, std::uint64_t{1}) == UINT64_MAX);
static_assert(saturating_sub(INT64_MIN, std::int64_t{1}) == INT64_MIN);
static_assert(saturating_mul(INT64_MIN, std::int64_t{-1}) == INT64_MAX);
static_assert(saturating_div(INT64_MIN, std::int64_t{-1}) == INT64_MAX);
static_assert(saturating_cast<std::uint8_t>(1000) == 255);
consteval bool hints() {
  const int value = 3;
  auto token = uarch::mem::PrefetchWrite::assume_supported();
  uarch::mem::prefetch_read(value);
  uarch::mem::prefetch_write(token, value);
  return uarch::mem::cache_line_offset(&value) == 0;
}
static_assert(hints());
int main() {}
