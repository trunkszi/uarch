#include <array>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>

using uarch::arithmetic::FastDivisorU64;
static_assert(std::is_trivially_copyable_v<FastDivisorU64>);
static_assert(!std::is_default_constructible_v<FastDivisorU64>);

void check(std::uint64_t n, std::uint64_t d) {
  const auto result = FastDivisorU64::try_new(d);
  if (!result) std::abort();
  const auto& divisor = *result;
  const auto [q, r] = divisor.div_rem(n);
  if (q != n / d || r != n % d || divisor.div(n) != q || divisor.rem(n) != r || divisor.is_multiple_of(n) != (r == 0) ||
      divisor.divisor() != d || divisor.is_power_of_two() != ((d & (d - 1)) == 0) || !(divisor == *result))
    std::abort();
}

static_assert(sizeof(FastDivisorU64) == 24);
int main() {
  for (const auto fill : {0ULL, ~0ULL}) {
    std::array<std::uint64_t, 3> raw{};
    raw.fill(fill);
    const auto forged = std::bit_cast<FastDivisorU64>(raw);
    const auto [q, rem] = forged.div_rem(UINT64_MAX);
    if (q != forged.div(UINT64_MAX) || rem != forged.rem(UINT64_MAX)) return 1;
  }
  const auto shifted = std::bit_cast<FastDivisorU64>(std::array<std::uint64_t, 3>{0, 0, UINT64_MAX});
  if (shifted.div(UINT64_MAX) != 1) return 1;
  const auto zero = FastDivisorU64::try_new(0);
  if (zero || zero.error() != uarch::arithmetic::DivisorError::Zero) return 1;
  for (const auto d : std::array<std::uint64_t, 5>{1, 2, 3, 7, UINT64_MAX})
    for (const auto n : std::array<std::uint64_t, 6>{0, 1, 2, 3, 7, UINT64_MAX}) check(n, d);
  for (unsigned bit = 0; bit < 64; ++bit) {
    const auto d = std::uint64_t{1} << bit;
    check(0, d);
    check(1, d);
    check(d - 1, d);
    check(d, d);
    check(UINT64_MAX, d);
  }
  std::uint64_t state = 0xa5a55a5aU;
  for (unsigned i = 0; i < 100000; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    const auto n = state;
    state = state * 1664525U + 1013904223U;
    check(n, state == 0 ? 1 : state);
  }
}
