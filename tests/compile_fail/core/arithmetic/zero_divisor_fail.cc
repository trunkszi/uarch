// expected-error: divisor must be non-zero
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
constexpr auto rejected = uarch::arithmetic::divisor_u64<0>();
