#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <uarch/detect/detect.hpp>
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
  auto before = uarch::detect::affinity_cpus();
  auto snapshot = uarch::detect::Snapshot::from_source(uarch::detect::HardwareCpuid{});
  std::array<long long, 101> samples{};
  unsigned checksum = 0;
  const auto start_allocations = allocations;
  for (auto& sample : samples) {
    auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 256; ++i) {
      asm volatile("" ::: "memory");
      checksum += static_cast<unsigned>(uarch::detect::classify(snapshot));
      asm volatile("" : "+r"(checksum));
    }
    sample = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
  }
  const auto measured_allocations = allocations - start_allocations;
  if (measured_allocations != 0) return 2;
  std::ranges::sort(samples);
  auto after = uarch::detect::affinity_cpus();
  if (before.has_value() != after.has_value() || (before && *before != *after)) return 1;
  std::printf("pure classification batch=256 p50=%lldns p90=%lldns p99=%lldns p99.9=%lldns checksum=%u allocations=%zu\n",
              samples[50], samples[90], samples[99], samples[100], checksum, measured_allocations);
  return 0;
}
