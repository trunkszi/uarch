#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/container/inplace_vector.hpp>
#include <uarch/core/simd/vec.hpp>

namespace {
std::size_t allocations = 0;
}
void* operator new(std::size_t n) {
  if (void* p = std::malloc(n == 0 ? 1 : n)) {
    ++allocations;
    return p;
  }
  std::abort();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] std::int64_t Sum(const std::int64_t* data) noexcept {
  using V = uarch::simd::vec<std::int64_t, 4>;
  auto input = uarch::simd::unchecked_load<V>(data);
  return uarch::simd::reduce(uarch::simd::select(input < std::int64_t{5}, input, V{0}));
}
int main() {
  std::array<long long, 101> samples{};
  std::array<std::int64_t, 4> values{1, 2, 3, 9};
  if (Sum(values.data()) != 6) return 1;
  std::uint64_t checksum = 0;
  const auto before = allocations;
  for (auto& sample : samples) {
    auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 256; ++i) {
      asm volatile("" ::: "memory");
      uarch::container::inplace_vector<std::int64_t, 4> inline_values;
      for (auto value : values) inline_values.unchecked_push_back(value);
      checksum += static_cast<std::uint64_t>(Sum(inline_values.data()));
      asm volatile("" : "+r"(checksum));
    }
    sample = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
  }
  if (allocations != before || checksum != 155136) return 2;
  std::ranges::sort(samples);
  std::printf("simd/container batch=256 p50=%lldns p90=%lldns p99=%lldns p99.9=%lldns allocations=%zu checksum=%llu\n",
              samples[50], samples[90], samples[99], samples[100], allocations - before,
              static_cast<unsigned long long>(checksum));
  return 0;
}
