#include <array>
#include <bit>
#include <type_traits>
#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/mem/pool_index.hpp>
using namespace uarch::mem;
struct Tag {
  Tag(const Tag&) = delete;
  bool operator==(const Tag&) const = delete;
};
static_assert(sizeof(PoolIndex<Tag>) == 4);
static_assert(sizeof(OptionalPoolIndex<Tag>) == 4);
static_assert(!std::is_default_constructible_v<PoolIndex<Tag>>);
static_assert(std::is_trivially_copyable_v<PoolIndex<Tag>>);
static_assert(std::is_trivially_copyable_v<OptionalPoolIndex<Tag>>);
static_assert(std::is_copy_constructible_v<PoolIndex<Tag>>);
static_assert(!std::is_trivially_copyable_v<Pool<int, 2>>);

static_assert(uarch::layout::NoPadding<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::RegisterLeaf<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::RegisterPassable<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::GprPair<uarch::mem::PoolIndex<int>>);
static_assert(uarch::layout::NoPadding<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::RegisterLeaf<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::RegisterPassable<uarch::mem::OptionalPoolIndex<int>>);
static_assert(uarch::layout::GprPair<uarch::mem::OptionalPoolIndex<int>>);

int main() {
  if (PoolIndex<int>::try_new(0)) return 1;
  const auto first = PoolIndex<Tag>::try_new(1);
  const auto next = PoolIndex<Tag>::try_new(2);
  if (!first || !next || !(*first < *next) || !(*first == *PoolIndex<Tag>::try_new(1))) return 1;
  const auto none = OptionalPoolIndex<Tag>::none();
  const auto some = OptionalPoolIndex<Tag>::some(*first);
  if (none.has_value() || none.get() || !some.has_value() || some.get()->slot() != 1) return 1;
  std::array<int, 4> storage{0, 10, 20, 30};
  {
    auto pool = Pool<int, 2>::try_new(storage);
    if (!pool || Pool<int, 2>::try_new(std::span<int>(storage).first(3))) return 1;
    const auto zero = std::bit_cast<PoolIndex<int>>(std::uint32_t{0});
    if (zero.slot() != 0 || pool->get(zero) != nullptr || &pool->get_masked(zero) != &storage[0]) return 1;
    const auto& const_pool = *pool;
    if (const_pool.get(zero) != nullptr || &const_pool.get_masked(zero) != &storage[0]) return 1;
    const auto forged_max = std::bit_cast<PoolIndex<int>>(UINT32_MAX);
    if (pool->get(forged_max) != nullptr || &pool->get_masked(forged_max) != &storage[3]) return 1;
    if (OptionalPoolIndex<int>::some(zero).has_value()) return 1;
    for (std::uint32_t slot = 0; slot < 8; ++slot) {
      const auto candidate = std::bit_cast<PoolIndex<int>>(slot);
      const auto expected = slot >= 1 && slot < 4 ? &storage[slot] : nullptr;
      if (pool->get(candidate) != expected || const_pool.get(candidate) != expected) return 2;
    }
    const auto index = *PoolIndex<int>::try_new(2);
    const auto outside = *PoolIndex<int>::try_new(5);
    const auto sentinel = *PoolIndex<int>::try_new(4);
    if (!pool->get(index) || *pool->get(index) != 20 || pool->get(outside) != nullptr) return 1;
    if (pool->get_masked(outside) != 10 || pool->get_masked(sentinel) != 0) return 1;
    pool->get_masked(index) = 7;
    const auto& view = *pool;
    if (*view.get(index) != 7 || view.get(outside) != nullptr || view.get_masked(outside) != 10) return 1;
  }
  return storage[2] != 7;
}
