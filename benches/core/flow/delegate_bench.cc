#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/flow/delegate.hpp>
namespace {
std::atomic<std::size_t> allocations{0};
struct Receiver {
  template <std::uint64_t Bias>
  [[gnu::noinline]] std::uint64_t apply(std::uint64_t value) noexcept {
    return value + Bias;
  }
};
using Callback = uarch::flow::Callback<std::uint64_t(std::uint64_t)>;
bool measure(bool heterogeneous) {
  constexpr std::size_t kBatch = 256, kSamples = 1024;
  Receiver receiver;
  const std::array targets{Callback::bind<&Receiver::apply<1>>(receiver), Callback::bind<&Receiver::apply<2>>(receiver),
                           Callback::bind<&Receiver::apply<3>>(receiver), Callback::bind<&Receiver::apply<4>>(receiver)};
  std::array<Callback, kBatch> callbacks{};
  std::uint64_t expected = 0;
  for (std::size_t i = 0; i < kBatch; ++i) {
    callbacks[i] = targets[heterogeneous ? i & 3 : 0];
    expected += i + (heterogeneous ? i & 3 : 0) + 1;
  }
  std::array<std::uint64_t, kSamples> samples{};
  const auto before = allocations.load(std::memory_order_relaxed);
  for (auto& sample : samples) {
    asm volatile("" : : "r"(callbacks.data()) : "memory");
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < kBatch; ++i) sum += callbacks[i](i);
    asm volatile("" : "+r"(sum));
    const auto end = std::chrono::steady_clock::now();
    if (sum != expected) return false;
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
  if (allocations.load(std::memory_order_relaxed) != before) return false;
  std::ranges::sort(samples);
  std::printf("%s batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu allocs=0\n", heterogeneous ? "heterogeneous" : "homogeneous",
              static_cast<unsigned long long>(samples[512]), static_cast<unsigned long long>(samples[921]),
              static_cast<unsigned long long>(samples[1013]), static_cast<unsigned long long>(samples[1022]));
  return true;
}
}  // namespace
void* operator new(std::size_t bytes) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* pointer = std::malloc(std::max(bytes, 1uz))) return pointer;
  std::abort();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
int main() { return !(measure(false) && measure(true)); }
