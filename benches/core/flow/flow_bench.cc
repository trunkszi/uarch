#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/flow/cold.hpp>
#include <uarch/core/flow/reduce_lanes.hpp>
namespace {
std::atomic<std::size_t> allocation_count{0};
template <class T>
T opaque(T x) {
  asm volatile("" : "+r"(x));
  return x;
}
template <bool Parallel>
void measure(const char* label) {
  std::array<std::uint64_t, 1024> samples{};
  std::array<std::uint64_t, 256> xs{};
  std::uint64_t state = 0x123456789abcdefULL, total = 0;
  auto plus = [](auto a, auto b) { return a + b; };
  for (auto& sample : samples) {
    std::uint64_t expected = 0;
    for (auto& x : xs) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      x = state;
      expected += x;
    }
    const auto before = allocation_count.load(std::memory_order_relaxed);
    asm volatile("" : : "r"(xs.data()) : "memory");
    auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = 0;
    if constexpr (Parallel)
      sum = uarch::flow::reduce_lanes<4>(std::span<const std::uint64_t>(xs), std::uint64_t{0}, std::uint64_t{0}, plus, plus);
    else
      for (auto x : xs) sum += x;
    sum = opaque(sum);
    auto end = std::chrono::steady_clock::now();
    if (sum != expected || allocation_count.load(std::memory_order_relaxed) != before) std::abort();
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    total += sample;
  }
  std::ranges::sort(samples);
  std::printf("%s batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu ops/s=%.0f new_calls=0\n", label,
              static_cast<unsigned long long>(samples[512]), static_cast<unsigned long long>(samples[921]),
              static_cast<unsigned long long>(samples[1013]), static_cast<unsigned long long>(samples[1022]),
              256.0 * 1024.0 * 1e9 / static_cast<double>(std::max(total, std::uint64_t{1})));
}
}  // namespace
void* operator new(std::size_t count) {
  allocation_count.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(std::max(count, std::size_t{1}))) return p;
  std::abort();
}
void* operator new[](std::size_t count) { return ::operator new(count); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

extern "C" [[gnu::noinline]] std::uint64_t t4_select(bool c, std::uint64_t a, std::uint64_t b) {
  return uarch::flow::select_register(uarch::flow::predicate(c), a, b);
}
constexpr std::uint64_t sink(std::uint64_t x) noexcept { return x + 1; }
extern "C" [[gnu::noinline]] std::uint64_t t4_hot(bool c, std::uint64_t x) {
  if (uarch::flow::unlikely(c)) return uarch::flow::cold_call<sink>(x);
  return x;
}
int main() {
  measure<false>("serial");
  measure<true>("reduce4");
}
