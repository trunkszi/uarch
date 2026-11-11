#include <uarch/detect/cpuid.hpp>
#if defined(__x86_64__)
#include <cpuid.h>
#include <immintrin.h>
#endif
namespace uarch::detect {
CpuidRegisters HardwareCpuid::cpuid(std::uint32_t leaf, std::uint32_t subleaf) const noexcept {
  static_cast<void>(*this == *this);
#if defined(__x86_64__)
  CpuidRegisters result;
  __cpuid_count(leaf, subleaf, result.eax, result.ebx, result.ecx, result.edx);
  return result;
#else
  (void)leaf;
  (void)subleaf;
  return {};
#endif
}
#if defined(__x86_64__)
namespace {
[[gnu::target("xsave")]] std::uint64_t ReadXcr0() noexcept { return static_cast<std::uint64_t>(_xgetbv(0)); }
}  // namespace
#endif
std::optional<std::uint64_t> HardwareCpuid::xcr0() const noexcept {
  static_cast<void>(*this == *this);
#if defined(__x86_64__)
  if (!available(*this, 1) || (cpuid(1, 0).ecx & (3U << 26)) != (3U << 26)) return std::nullopt;
  return ReadXcr0();
#else
  return std::nullopt;
#endif
}
}  // namespace uarch::detect
