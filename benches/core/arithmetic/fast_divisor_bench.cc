#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>

namespace {
std::atomic<std::size_t> allocation_count{0};
template <class T>
T opaque(T value) noexcept {
  // Identical input/output barriers prevent either variant from folding the corpus.
  asm volatile("" : "+r"(value));
  return value;
}
std::uint64_t next(std::uint64_t& state) noexcept {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

template <class Divisor, bool Fast>
void measure(const char* label) {
  using Word = decltype(std::declval<const Divisor&>().divisor());
  const auto divisor = Divisor::try_new(opaque(Word{7}));
  if (!divisor) std::abort();
  const Word raw = divisor->divisor();
  std::array<std::uint64_t, 1024> samples{};
  std::array<Word, 256> corpus{};
  std::uint64_t seed = 0x0123456789abcdefULL;
  std::uint64_t total = 0;
  for (auto& sample : samples) {
    std::uint64_t expected = 0;
    for (auto& n : corpus) {
      n = static_cast<Word>(next(seed));
      expected += n / raw;
    }
    std::uint64_t observed = 0;
    const auto allocations_before = allocation_count.load(std::memory_order_relaxed);
    const auto start = std::chrono::steady_clock::now();
    for (auto n : corpus) {
      n = opaque(n);
      if constexpr (Fast)
        observed += opaque(divisor->div(n));
      else
        observed += opaque(n / raw);
    }
    const auto end = std::chrono::steady_clock::now();
    if (allocation_count.load(std::memory_order_relaxed) != allocations_before || observed != expected) std::abort();
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    total += sample;
  }
  std::ranges::sort(samples);
  std::printf("%s batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu ops/s=%.0f new_calls=0\n", label,
              static_cast<unsigned long long>(samples[512]), static_cast<unsigned long long>(samples[921]),
              static_cast<unsigned long long>(samples[1013]), static_cast<unsigned long long>(samples[1022]),
              (256.0 * 1024.0 * 1e9) / static_cast<double>(std::max(total, std::uint64_t{1})));
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

extern "C" [[gnu::noinline]] std::uint64_t t5_div_u64(std::uint64_t n, const uarch::arithmetic::FastDivisorU64* d) noexcept {
  return d->div(n);
}
extern "C" [[gnu::noinline]] std::uint32_t t5_div_u32(std::uint32_t n, const uarch::arithmetic::FastDivisorU32* d) noexcept {
  return d->div(n);
}

extern "C" [[gnu::noinline]] void t5_div_rem_u32(std::uint32_t n, const uarch::arithmetic::FastDivisorU32* d,
                                                 std::uint32_t* quotient, std::uint32_t* remainder) noexcept {
  const auto parts = d->div_rem(n);
  *quotient = parts.first;
  *remainder = parts.second;
}
extern "C" [[gnu::noinline]] void t5_div_rem_u64(std::uint64_t n, const uarch::arithmetic::FastDivisorU64* d,
                                                 std::uint64_t* quotient, std::uint64_t* remainder) noexcept {
  const auto parts = d->div_rem(n);
  *quotient = parts.first;
  *remainder = parts.second;
}

int main() {
  measure<uarch::arithmetic::FastDivisorU32, false>("u32/hardware");
  measure<uarch::arithmetic::FastDivisorU32, true>("u32/reciprocal");
  measure<uarch::arithmetic::FastDivisorU64, false>("u64/hardware");
  measure<uarch::arithmetic::FastDivisorU64, true>("u64/reciprocal");
}
