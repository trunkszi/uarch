// expected-error: power-of-two shift must be < 64
#include <uarch/core/arithmetic/power_of_two.hpp>
static_assert(sizeof(uarch::arithmetic::PowerOfTwo<64>) > 0);
