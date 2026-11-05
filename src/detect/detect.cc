#include <string>
#include <uarch/detect/detect.hpp>
#if defined(__linux__) && defined(__aarch64__)
#include <sys/auxv.h>
#endif
namespace uarch::detect {
namespace {
class DetectCategory final : public std::error_category {
 public:
  [[nodiscard]] const char* name() const noexcept override { return "uarch.detect"; }
  [[nodiscard]] std::string message(int value) const override {
    switch (value) {
      case 1:
        return "the current execution domain could not be verified";
      case 2:
        return "the requested instruction is unsupported";
      default:
        return "unknown detection error";
    }
  }
};
class AuditCategory final : public std::error_category {
 public:
  [[nodiscard]] const char* name() const noexcept override { return "uarch.audit"; }
  [[nodiscard]] std::string message(int value) const override {
    switch (value) {
      case 1:
        return "host evidence is unavailable";
      case 2:
        return "host evidence is malformed";
      default:
        return "unknown host audit error";
    }
  }
};
}  // namespace
std::error_code make_error_code(DetectError error) noexcept {
  static const DetectCategory category;
  return {static_cast<int>(error) + 1, category};
}
std::error_code make_error_code(AuditError error) noexcept {
  static const AuditCategory category;
  return {static_cast<int>(error) + 1, category};
}
std::expected<arch::Tier, DetectError> detect(DetectPolicy policy) noexcept {
  auto id = tiers::TierId::Scalar;
#if defined(__x86_64__)
  auto domain = execution_domain();
  if (!domain) {
    if (policy.domain == ExecutionDomainPolicy::RequireHomogeneous)
      return std::unexpected(DetectError::UnverifiedExecutionDomain);
  } else {
    if (domain->sources.empty()) return std::unexpected(DetectError::UnverifiedExecutionDomain);
    if (policy.domain == ExecutionDomainPolicy::RequireHomogeneous) {
      for (const auto& source : domain->sources)
        if (!same_isa(domain->sources.front(), source)) return std::unexpected(DetectError::UnverifiedExecutionDomain);
    }
    id = classify_domain<Snapshot>(domain->sources, policy);
  }
#elif defined(__linux__) && defined(__aarch64__)
  if ((getauxval(AT_HWCAP) & (1UL << 1)) != 0)
    id = tiers::TierId::Aarch64Neon;
  else if (policy.domain == ExecutionDomainPolicy::RequireHomogeneous)
    return std::unexpected(DetectError::UnverifiedExecutionDomain);
#else
  if (policy.domain == ExecutionDomainPolicy::RequireHomogeneous)
    return std::unexpected(DetectError::UnverifiedExecutionDomain);
#endif
  // Only complete real evidence can choose a non-scalar ID on this issuance path.
  return arch::detail::TokenFactory::make_tier(id);
}
std::expected<arch::PrefetchWrite, DetectError> detect_prefetch_write() noexcept {
#if defined(__x86_64__)
  auto domain = execution_domain();
  if (!domain || domain->sources.empty()) return std::unexpected(DetectError::UnverifiedExecutionDomain);
  for (const auto& source : domain->sources)
    if (!prefetch_write_supported(source)) return std::unexpected(DetectError::Unsupported);
  return arch::PrefetchWrite::assume_supported();
#elif defined(__aarch64__)
  return arch::PrefetchWrite::assume_supported();
#else
  return std::unexpected(DetectError::Unsupported);
#endif
}
}  // namespace uarch::detect
