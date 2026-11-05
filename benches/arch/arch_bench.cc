#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/arch/entry.hpp>
#if defined(__x86_64__)
#include <cpuid.h>
#endif
using namespace uarch::arch;
struct Gather {
  [[gnu::always_inline]] std::uint64_t operator()(Scalar a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3SlowPdep a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::uint64_t operator()(X86V4 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] std::uint64_t operator()(Aarch64Neon a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
};
namespace {
std::atomic<std::size_t> allocation_count{0};
template <class T>
T opaque(T x) {
  asm volatile("" : "+r"(x));
  return x;
}
template <Arch A>
void measure(const char* label, A token) {
  std::array<std::uint64_t, 1024> samples{};
  std::array<std::uint64_t, 128> corpus{};
  std::uint64_t state = 0x123456789abcdefULL, total = 0;
  for (auto& sample : samples) {
    std::uint64_t expected = 0;
    for (auto& x : corpus) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      x = state;
      expected += Scalar{}.bit_gather(x, 0x5555555555555555ULL);
    }
    std::uint64_t observed = 0;
    const auto before = allocation_count.load(std::memory_order_relaxed);
    const auto start = std::chrono::steady_clock::now();
    for (auto x : corpus) observed += opaque(run<Gather{}>(token, opaque(x), opaque(std::uint64_t{0x5555555555555555ULL})));
    const auto end = std::chrono::steady_clock::now();
    if (observed != expected || before != allocation_count.load(std::memory_order_relaxed)) std::abort();
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    total += sample;
  }
  std::ranges::sort(samples);
  std::printf("%s batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu ops/s=%.0f new_calls=0\n", label,
              static_cast<unsigned long long>(samples[512]), static_cast<unsigned long long>(samples[921]),
              static_cast<unsigned long long>(samples[1013]), static_cast<unsigned long long>(samples[1022]),
              128.0 * 1024.0 * 1e9 / static_cast<double>(std::max(total, std::uint64_t{1})));
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

#if defined(__x86_64__)
bool host_v3() {
  __builtin_cpu_init();
  unsigned a = 0, b = 0, c = 0, d = 0;
  constexpr unsigned required = (1U << 0) | (1U << 9) | (1U << 13) | (1U << 19) | (1U << 20) | (1U << 26) | (1U << 27);
  if (!__get_cpuid(1, &a, &b, &c, &d) || (c & required) != required) return false;
  return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2") &&
         __builtin_cpu_supports("fma") && __builtin_cpu_supports("f16c") && __builtin_cpu_supports("lzcnt") &&
         __builtin_cpu_supports("movbe") && __builtin_cpu_supports("popcnt");
}
bool host_v4() {
  return host_v3() && __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") &&
         __builtin_cpu_supports("avx512cd") && __builtin_cpu_supports("avx512dq") && __builtin_cpu_supports("avx512vl");
}
#endif
int main() {
  measure("scalar", Scalar{});
#if defined(__x86_64__)
  if (host_v3()) {
    measure("v3", X86V3::assume_supported(unchecked_tag));
    measure("v3-slow-pdep", X86V3SlowPdep::assume_supported(unchecked_tag));
  }
#endif
#if defined(__x86_64__)
  if (host_v4()) measure("v4", X86V4::assume_supported(unchecked_tag));
#endif
#if defined(__aarch64__)
  measure("neon", Aarch64Neon::assume_supported(unchecked_tag));
#endif
}
