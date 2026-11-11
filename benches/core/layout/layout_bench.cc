#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/isolated.hpp>
#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/mem/pool_index.hpp>

namespace {
std::atomic<std::size_t> allocations{0};
}
struct RegisterPair {
  std::uint64_t lo, hi;
};
static_assert(uarch::layout::GprPair<RegisterPair>);
extern "C" [[gnu::noinline]] RegisterPair t2_register_pair(RegisterPair input) noexcept { return {input.lo + 1, input.hi + 2}; }

void* operator new(std::size_t count) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (auto* pointer = std::malloc(std::max(count, std::size_t{1}))) return pointer;
  std::abort();
}
void* operator new[](std::size_t count) { return ::operator new(count); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
  std::array<std::uint64_t, 8> slots{0, 1, 2, 3, 4, 5, 6, 7};
  auto pool = uarch::mem::Pool<std::uint64_t, 3>::try_new(slots);
  if (!pool) return 1;
  uarch::layout::CacheAligned<std::uint64_t> cache{0};
  uarch::layout::Isolated<std::uint64_t> isolated{0};
  std::array<std::uint64_t, 1024> samples{};
  std::uint64_t seed = 0x123456789abcdef0ULL;
  std::uint64_t total = 0;
  for (auto& sample : samples) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    const auto index = uarch::mem::PoolIndex<std::uint64_t>::try_new(static_cast<std::uint32_t>(seed) | 1U);
    if (!index) return 1;
    const auto count = allocations.load(std::memory_order_relaxed);
    auto input = seed;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 256; ++i) {
      asm volatile("" : "+r"(input));
      const auto pair = t2_register_pair({input, input + 1});
      *cache = pair.lo;
      *isolated = pair.hi;
      pool->get_masked(*index) = *cache + *isolated;
      input = *cache;
    }
    const auto end = std::chrono::steady_clock::now();
    if (allocations.load(std::memory_order_relaxed) != count || input != seed + 256 ||
        pool->get_masked(*index) != (seed + 256) + (seed + 258))
      return 1;
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    total += sample;
  }
  std::ranges::sort(samples);
  std::printf("layout/pool batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu ops/s=%.0f new_calls=0\n",
              static_cast<unsigned long long>(samples[512]), static_cast<unsigned long long>(samples[921]),
              static_cast<unsigned long long>(samples[1013]), static_cast<unsigned long long>(samples[1022]),
              (256.0 * 1024.0 * 1e9) / static_cast<double>(std::max(total, std::uint64_t{1})));
}
