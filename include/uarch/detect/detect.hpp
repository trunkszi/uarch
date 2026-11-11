/** @file @brief Real-evidence capability issuance; pure classification cannot issue permits. */
#pragma once
#include <expected>
#include <uarch/arch/tier.hpp>
#include <uarch/detect/error.hpp>
#include <uarch/detect/host.hpp>
#include <uarch/detect/policy.hpp>

namespace uarch::detect {
/** @brief Audit the full caller affinity domain before minting a tier.
 * Preserve the audited domain and OS ISA state while the permit is live, including on receiving threads.
 * Multi-CPU x86 Linux needs readable /dev/cpu/N/cpuid devices for every allowed CPU.
 * Unknown evidence falls back to Scalar only with CommonSubset; this function never changes affinity.
 */
[[nodiscard]] std::expected<arch::Tier, DetectError> detect(DetectPolicy policy = {}) noexcept;
/** @brief Verify PREFETCHW on every allowed x86 CPU; missing evidence has no scalar fallback. */
[[nodiscard]] std::expected<arch::PrefetchWrite, DetectError> detect_prefetch_write() noexcept;
}  // namespace uarch::detect
