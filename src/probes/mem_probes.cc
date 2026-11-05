#include <uarch/core/mem/pool_index.hpp>
#include <uarch/core/mem/prefetch.hpp>
extern "C" [[gnu::noinline]] void pool_prefetch_l1(const std::uint8_t* ptr) { uarch::mem::prefetch_read(*ptr); }
extern "C" [[gnu::noinline]] void uarch_arch_prefetch_write(const std::uint8_t* ptr) {
  uarch::mem::prefetch_write(uarch::mem::PrefetchWrite::assume_supported(), *ptr);
}

extern "C" [[gnu::noinline]] std::uint64_t pool_get_masked(std::uint64_t* base, uarch::mem::PoolIndex<std::uint64_t> index) {
  return uarch::mem::Pool<std::uint64_t, 8>::try_new(std::span<std::uint64_t>(base, 256))->get_masked(index);
}
