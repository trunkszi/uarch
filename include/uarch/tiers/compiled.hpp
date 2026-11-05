/** @file @brief Compile-target description without host detection or ISA execution authority. */
#pragma once

#include <uarch/tiers/tier_id.hpp>

namespace uarch::tiers {
/** @brief Tier guaranteed by compiler target flags, never a runtime capability token. */
[[nodiscard]] constexpr TierId compiled_tier() noexcept {
#if defined(__x86_64__) && defined(__AVX__) && defined(__AVX2__) && defined(__BMI__) && defined(__BMI2__) &&       \
    defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16) && defined(__F16C__) && defined(__FMA__) && defined(__LZCNT__) && \
    defined(__MOVBE__) && defined(__POPCNT__) && defined(__SSE3__) && defined(__SSSE3__) && defined(__SSE4_1__) && \
    defined(__SSE4_2__) && defined(__XSAVE__) && defined(__LAHF_SAHF__)
#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512CD__) && defined(__AVX512DQ__) && defined(__AVX512VL__)
  return TierId::X86V4;
#else
  return TierId::X86V3;
#endif
#elif defined(__aarch64__) && defined(__ARM_NEON)
  return TierId::Aarch64Neon;
#else
  return TierId::Scalar;
#endif
}
}  // namespace uarch::tiers
