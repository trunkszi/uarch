#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/core/layout/soa_columns.hpp>
struct Record {
  std::uint64_t timestamp, count;
};
namespace {
std::atomic<std::size_t> allocation_count{0};
template <class T>
T opaque(T x) {
  asm volatile("" : "+r"(x));
  return x;
}
template <bool Column>
void measure(const char* name) {
  std::array<Record, 256> rows{};
  uarch::layout::SoaColumns<Record, 256> columns;
  std::array<std::uint64_t, 1024> samples{};
  std::uint64_t state = 0x123456789abcdefULL, total = 0;
  for (auto& sample : samples) {
    std::uint64_t expected = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      rows[i] = {state, state ^ 123};
      columns.set_row(i, rows[i]);
      expected += state;
    }
    asm volatile("" : : "r"(rows.data()), "r"(columns.column_data<^^Record::timestamp>()) : "memory");
    auto before = allocation_count.load(std::memory_order_relaxed);
    auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = 0;
    if constexpr (Column) {
      for (auto timestamp : columns.column<^^Record::timestamp>()) sum += timestamp;
    } else {
      for (auto row : rows) sum += row.timestamp;
    }
    sum = opaque(sum);
    auto end = std::chrono::steady_clock::now();
    if (sum != expected || allocation_count.load(std::memory_order_relaxed) != before) std::abort();
    sample = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    total += sample;
  }
  std::ranges::sort(samples);
  std::printf("%s batch_ns P50=%llu P90=%llu P99=%llu P99.9=%llu ops/s=%.0f new_calls=0\n", name,
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

int main() {
  measure<false>("aos/timestamp");
  measure<true>("soa/timestamp");
}
