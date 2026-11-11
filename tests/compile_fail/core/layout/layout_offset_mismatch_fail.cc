// expected-error: expected byte 4
#include <uarch/core/layout/assertions.hpp>
struct Pair {
  std::uint64_t a, b;
};
static_assert(uarch::layout::assert_layout<Pair, 16, 8, 0, 4>());
