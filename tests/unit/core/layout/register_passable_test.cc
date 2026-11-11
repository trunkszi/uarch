#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/mem/pool_index.hpp>
using namespace uarch::layout;
struct Pair {
  std::uint64_t a, b;
};
struct Floating {
  double a, b;
};
struct Mixed {
  double a;
  std::uint64_t b;
};
struct Wide {
  __uint128_t value;
};
struct Plain {
  std::uint64_t a;
};
struct[[= register_leaf]] Leaf {
  std::uint64_t a;
};
struct[[= register_leaf]] BadLeaf {
  std::uint32_t a, b;
};
struct[[= register_leaf]] Nested {
  Leaf a;
};
static_assert(GprPair<Wide> && !RegisterLeaf<Wide>);
static_assert(RegisterPassable<Pair> && GprPair<Pair>);
static_assert(RegisterPassable<Floating> && !GprPair<Floating>);
static_assert(RegisterPassable<Mixed> && !GprPair<Mixed>);
static_assert(RegisterLeaf<std::uint64_t> && RegisterLeaf<double>);
using Alias = Leaf;
static_assert(RegisterLeaf<Alias>);
static_assert(RegisterLeaf<Leaf> && RegisterLeaf<Nested>);
static_assert(!RegisterLeaf<Plain> && !RegisterLeaf<BadLeaf>);
static_assert(!RegisterLeaf<__uint128_t> && !RegisterLeaf<std::uint8_t[8]>);
static_assert(!RegisterLeaf<bool> && !RegisterPassable<bool>);

static_assert(uarch::layout::NoPadding<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::RegisterLeaf<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::RegisterPassable<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::GprPair<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::NoPadding<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::RegisterLeaf<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::RegisterPassable<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::GprPair<uarch::mem::OptionalPoolIndex<int>>);

int main() {}
