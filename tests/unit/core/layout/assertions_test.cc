#include <uarch/core/layout/assertions.hpp>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/isolated.hpp>
struct Incomplete;
union Union {
  std::uint64_t x, y;
};
class Hidden {
  [[maybe_unused]] std::uint64_t a, b;
};
struct Pair {
  std::uint64_t a, b;
};
struct Padded {
  char a;
  std::uint64_t b;
};
struct Bits {
  unsigned value : 1;
};
struct Base {
  int value;
};
struct Derived : Base {};
using namespace uarch::layout;
static_assert(assert_layout<Pair, 16, 8, 0, 8>());
static_assert(assert_layout<Hidden, 16, 8, 0, 8>());
static_assert(!check_layout<Union, 8, 8>());
static_assert(!check_layout<Pair, 16, 8, 0, 4>());
static_assert(!check_layout<int, sizeof(int)>());
static_assert(!check_layout<Pair, 8>() && !check_layout<Pair, 16, 4>());
static_assert(!check_layout<Pair, 16, 8, 0>());
static_assert(!check_layout<Bits, sizeof(Bits)>() && !check_layout<Derived, sizeof(Derived)>());
static_assert(check_layout<Padded, sizeof(Padded), alignof(Padded), 0, 8>());
static_assert([] consteval {
  constexpr auto kReport = layout_report<Pair, 16, 8, 0, 4>();
  constexpr std::string_view kExpected{"Pair member 'b' at byte 8: expected byte 4"};
  if (kReport.size() != kExpected.size()) return false;
  for (std::size_t i = 0; i < kReport.size(); ++i)
    if (kReport[i] != kExpected[i]) return false;
  return true;
}());
static_assert(CacheLineSized<CacheAligned<int>> && !CacheLineDense<CacheAligned<int>>);
static_assert(assert_cache_line<CacheAligned<int>>() && assert_isolated<Isolated<int>>());
static_assert(assert_register_pass<Pair>() && assert_gpr_pair<Pair>());
int main() {}
