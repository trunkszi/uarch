#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/views/bit_ones.hpp>

namespace {
std::size_t allocations = 0;
}
void* operator new(std::size_t size) {
  if (void* p = std::malloc(size == 0 ? 1 : size)) {
    ++allocations;
    return p;
  }
  std::abort();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
int main() {
  std::array<long long, 101> samples{};
  constexpr unsigned kBatch = 256;
  std::uint64_t checksum = 0;
  const auto before = allocations;
  for (auto& sample : samples) {
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < kBatch; ++i) {
      std::uint64_t bits = 0x8100400010010089ULL;
      asm volatile("" : "+r"(bits));
      for (auto bit : bits | uarch::views::ones) checksum += bit;
      asm volatile("" : "+r"(checksum));
    }
    sample = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
  }
  if (allocations != before || checksum == 0) return 1;
  std::ranges::sort(samples);
  std::printf("bit traversal batch=%u p50=%lldns p90=%lldns p99=%lldns p99.9=%lldns allocations=%zu checksum=%llu\n", kBatch,
              samples[50], samples[90], samples[99], samples[100], allocations - before,
              static_cast<unsigned long long>(checksum));
  return 0;
}
