#include <array>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <uarch/core/arithmetic/fast_divisor_u32.hpp>

using uarch::arithmetic::FastDivisorU32;
static_assert(std::is_trivially_copyable_v<FastDivisorU32>);
static_assert(!std::is_default_constructible_v<FastDivisorU32>);

void check(std::uint32_t n, std::uint32_t d) {
  const auto result = FastDivisorU32::try_new(d);
  if (!result) std::abort();
  const auto& divisor = *result;
  const auto [q, r] = divisor.div_rem(n);
  if (q != n / d || r != n % d || divisor.div(n) != q || divisor.rem(n) != r || divisor.is_multiple_of(n) != (r == 0) ||
      divisor.divisor() != d || divisor.is_power_of_two() != ((d & (d - 1)) == 0) || !(divisor == *result))
    std::abort();
}

static_assert(sizeof(FastDivisorU32) == 16);
int main() {
  for (const auto fill : {0ULL, ~0ULL}) {
    std::array<std::uint64_t, 2> raw{};
    raw.fill(fill);
    const auto forged = std::bit_cast<FastDivisorU32>(raw);
    const auto [q, rem] = forged.div_rem(UINT32_MAX);
    if (q != forged.div(UINT32_MAX)) return 1;
    volatile auto observed = rem ^ forged.rem(UINT32_MAX);
    (void)observed;
  }
  const auto zero = FastDivisorU32::try_new(0);
  if (zero || zero.error() != uarch::arithmetic::DivisorError::Zero) return 1;
  for (const auto d : {1U, 2U, 3U, 7U, UINT32_MAX})
    for (const auto n : {0U, 1U, 2U, 3U, 7U, UINT32_MAX}) check(n, d);
  for (unsigned bit = 0; bit < 32; ++bit) {
    const auto d = std::uint32_t{1} << bit;
    check(d - 1, d);
    check(d, d);
    check(UINT32_MAX, d);
  }
  std::uint32_t state = 0xa5a55a5aU;
  for (unsigned i = 0; i < 100000; ++i) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    const auto n = state;
    state = state * 1664525U + 1013904223U;
    check(n, state == 0 ? 1 : state);
  }
}
