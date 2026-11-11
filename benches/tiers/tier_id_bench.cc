#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/tiers/tier_id.hpp>

namespace {
std::size_t allocation_count = 0;
}

void* operator new(std::size_t size) {
  if (void* storage = std::malloc(size == 0 ? 1 : size)) {
    ++allocation_count;
    return storage;
  }
  std::abort();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }

int main() {
  constexpr std::size_t kSamples = 101;
  constexpr unsigned kBatch = 4096;
  std::array<long long, kSamples> times{};
  std::uint64_t checksum = 0;
  const auto allocations_before = allocation_count;
  for (std::size_t sample = 0; sample < kSamples; ++sample) {
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < kBatch; ++i) {
      auto tier = static_cast<uarch::tiers::TierId>(i % 5);
      asm volatile("" : "+r"(tier));
      auto bits = uarch::tiers::max_vector_bits(tier);
      if (!bits) return 1;
      checksum += *bits;
      asm volatile("" : "+r"(checksum));
    }
    times[sample] = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
  }
  for (std::size_t i = 1; i < times.size(); ++i) {
    auto value = times[i];
    std::size_t j = i;
    while (j != 0 && times[j - 1] > value) {
      times[j] = times[j - 1];
      --j;
    }
    times[j] = value;
  }
  const auto allocations = allocation_count - allocations_before;
  if (allocations != 0) return 2;
  std::printf("tier metadata: batch=%u p50=%lldns p90=%lldns p99=%lldns p99.9=%lldns checksum=%llu allocations=%zu\n", kBatch,
              times[50], times[90], times[99], times[100], static_cast<unsigned long long>(checksum), allocations);
  return checksum == 0;
}
