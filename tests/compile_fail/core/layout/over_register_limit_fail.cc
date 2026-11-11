// expected-error: exceeds register limit
#include <uarch/core/layout/register_passable.hpp>
struct Large {
  std::uint64_t a, b, c;
};
static_assert(uarch::layout::RegisterPassable<Large>, "exceeds register limit");
