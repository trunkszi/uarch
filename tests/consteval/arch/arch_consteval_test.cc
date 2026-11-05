#include <array>
#include <limits>
#include <uarch/arch/token.hpp>
using namespace uarch::arch;
constexpr bool check() {
  constexpr Scalar scalar;
  constexpr std::array<std::int64_t, 4> keys{-5, 0, 7, 9};
  constexpr std::array<std::int64_t, 4> vals{INT64_MAX, 1, 3, 4};
  return scalar.bit_gather(0xA0, 0xF0) == 10 && scalar.bit_scatter(10, 0xF0) == 0xA0 &&
         scalar.masked_sum_lt_i64x4(keys, vals, 1) == INT64_MIN && scalar.find_first_ge_i64x4(keys, 1) == 2;
}
static_assert(check());
static_assert(X86V3::bit_gather_constexpr(0xA0, 0xF0) == 10);
static_assert(X86V4::bit_scatter_constexpr(10, 0xF0) == 0xA0);
int main() {}
