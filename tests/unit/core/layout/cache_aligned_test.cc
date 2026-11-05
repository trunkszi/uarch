#include <concepts>
#include <cstdint>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/no_padding.hpp>
using W = uarch::layout::CacheAligned<std::uint64_t>;
static_assert(alignof(W) == 64);
static_assert(sizeof(W) == 64);
static_assert(!uarch::layout::NoPadding<W>);
static_assert(std::same_as<decltype(std::declval<W&>().get()), std::uint64_t&>);
static_assert(std::same_as<decltype(std::declval<const W&>().get()), const std::uint64_t&>);
static_assert(std::same_as<decltype(*std::declval<W&&>()), std::uint64_t&&>);
static_assert(std::same_as<decltype(*std::declval<const W&&>()), const std::uint64_t&&>);
static_assert(std::same_as<decltype(std::declval<const W&>().data()), const std::uint64_t*>);
struct Item {
  int number;
};
int main() {
  W wrapper{7};
  wrapper.get() = 8;
  *wrapper = 9;
  if (*wrapper.data() != 9 || reinterpret_cast<std::uintptr_t>(wrapper.data()) % 64 != 0) return 1;
  const W copied{9};
  if (wrapper != copied || !(W{8} < copied)) return 1;
  uarch::layout::CacheAligned<Item> item{Item{4}};
  return item->number != 4;
}
