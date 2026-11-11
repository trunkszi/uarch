#include <array>
#if defined(__x86_64__)
#include <cpuid.h>
#endif
#include <type_traits>
#include <uarch/core/mem/prefetch.hpp>
using namespace uarch::mem;
static_assert(uarch::layout::ZeroRegisterCapability<PrefetchWrite> && !uarch::layout::NoPadding<PrefetchWrite>);
static_assert(!std::is_default_constructible_v<PrefetchWrite>);
static_assert(!std::is_trivially_copyable_v<PrefetchWrite>);
int main() {
  alignas(64) std::array<unsigned char, 128> data{};
  bool write_supported = true;
#if defined(__x86_64__)
  unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
  write_supported = __get_cpuid(0x80000001U, &eax, &ebx, &ecx, &edx) && (ecx & (1U << 8)) != 0;
#endif
  auto token = PrefetchWrite::assume_supported();
  for (std::size_t i = 0; i < data.size(); ++i) {
    prefetch_read<Locality::None>(data[i]);
    prefetch_read<Locality::Low>(data[i]);
    prefetch_read<Locality::Moderate>(data[i]);
    prefetch_read(data[i]);
    if (write_supported) {
      prefetch_write<Locality::None>(token, data[i]);
      prefetch_write<Locality::Low>(token, data[i]);
      prefetch_write<Locality::Moderate>(token, data[i]);
      prefetch_write(token, data[i]);
    }
    if (data[i] != 0 || cache_line_offset(&data[i]) != i % 64) return 1;
  }
}

static_assert(L1 == Locality::High && L2 == Locality::Moderate && L3 == Locality::Low && NonTemporal == Locality::None);
