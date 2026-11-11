#include <array>
#include <expected>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/isolated.hpp>
#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/mem/pool_index.hpp>
struct Pair {
  std::uint64_t a, b;
};
static_assert((uarch::layout::assert_no_padding<Pair>(), true));
static_assert(uarch::layout::NoPadding<uarch::layout::CacheAligned<std::uint64_t[8]>>);
static_assert(uarch::layout::NoPadding<uarch::layout::Isolated<std::uint64_t[16]>>);
consteval bool access() {
  uarch::layout::CacheAligned<int> cache{2};
  uarch::layout::Isolated<int> isolated{3};
  *cache = 4;
  if (*cache.data() != 4 || *isolated.data() != 3) return false;
  std::array<int, 4> slots{0, 1, 2, 3};
  auto pool = uarch::mem::Pool<int, 2>::try_new(slots);
  auto index = uarch::mem::PoolIndex<int>::try_new(2);
  if (!pool || !index) return false;
  pool->get_masked(*index) = 9;
  auto optional = uarch::mem::OptionalPoolIndex<int>::some(*index);
  return *pool->get(*index) == 9 && optional.has_value() && optional.get()->slot() == 2;
}
static_assert(access());
#if defined(__FLT16_MANT_DIG__)
struct HasFp16 {
  _Float16 value;
};
static_assert(!uarch::layout::NoPadding<_Float16> && !uarch::layout::NoPadding<HasFp16>);
#endif
struct alignas(16) OverAlignedFloat {
  float value;
};
struct Empty {};
struct EmptyAndDouble {
  Empty empty;
  double value;
};
static_assert(uarch::layout::aapcs64_classify<OverAlignedFloat>() ==
              uarch::layout::AbiShape{uarch::layout::AbiClass::Integer, uarch::layout::AbiClass::Integer, 2});
static_assert(uarch::layout::aapcs64_classify<EmptyAndDouble>() ==
              uarch::layout::AbiShape{uarch::layout::AbiClass::Integer, uarch::layout::AbiClass::Integer, 2});
struct NuaFloatPair {
  [[no_unique_address]] Empty empty;
  float a;
  float b;
};
static_assert(sizeof(NuaFloatPair) == 8);
static_assert(uarch::layout::aapcs64_classify<NuaFloatPair>() ==
              uarch::layout::AbiShape{uarch::layout::AbiClass::Sse, uarch::layout::AbiClass::Sse, 2});
int main() {}

#include <uarch/core/layout/assertions.hpp>
static_assert(uarch::layout::assert_layout<Pair, 16, 8, 0, 8>());
static_assert(uarch::layout::assert_cache_line<uarch::layout::CacheAligned<int>>());
static_assert(uarch::layout::assert_isolated<uarch::layout::Isolated<int>>());

struct AnonymousUnion {
  union {
    int integer;
    double real;
  };
};
static_assert(!uarch::layout::NoPadding<AnonymousUnion>);
static_assert(!uarch::layout::RegisterPassable<std::expected<int, int>>);
static_assert(!uarch::layout::padding_report<std::expected<int, int>>().empty());
