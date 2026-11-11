#include <limits>
#include <uarch/arch/entry.hpp>
#include <uarch/detect/detect.hpp>

struct Check {
  [[gnu::always_inline]] bool operator()(uarch::arch::Scalar token, const std::int64_t* keys,
                                         const std::int64_t* values) const noexcept {
    for (auto value : std::array<std::uint64_t, 4>{0, 1, ~std::uint64_t{0}, 0x123456789abcdefULL})
      for (auto mask : std::array<std::uint64_t, 4>{0, 1ULL << 63, 0x5555555555555555ULL, ~std::uint64_t{0}})
        if (token.bit_scatter(token.bit_gather(value, mask), mask) != (value & mask)) return false;
    return token.masked_sum_lt_i64x4(std::span<const std::int64_t, 4>{keys, 4}, std::span<const std::int64_t, 4>{values, 4},
                                     0) == std::numeric_limits<std::int64_t>::min() &&
           token.find_first_ge_i64x4(std::span<const std::int64_t, 4>{keys, 4}, 0) == 2;
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] bool operator()(uarch::arch::X86V3 token, const std::int64_t* keys,
                                                                        const std::int64_t* values) const noexcept {
    for (auto value : std::array<std::uint64_t, 4>{0, 1, ~std::uint64_t{0}, 0x123456789abcdefULL})
      for (auto mask : std::array<std::uint64_t, 4>{0, 1ULL << 63, 0x5555555555555555ULL, ~std::uint64_t{0}})
        if (token.bit_scatter(token.bit_gather(value, mask), mask) != (value & mask)) return false;
    return token.masked_sum_lt_i64x4(std::span<const std::int64_t, 4>{keys, 4}, std::span<const std::int64_t, 4>{values, 4},
                                     0) == std::numeric_limits<std::int64_t>::min() &&
           token.find_first_ge_i64x4(std::span<const std::int64_t, 4>{keys, 4}, 0) == 2;
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] bool operator()(uarch::arch::X86V3SlowPdep token,
                                                                        const std::int64_t* keys,
                                                                        const std::int64_t* values) const noexcept {
    for (auto value : std::array<std::uint64_t, 4>{0, 1, ~std::uint64_t{0}, 0x123456789abcdefULL})
      for (auto mask : std::array<std::uint64_t, 4>{0, 1ULL << 63, 0x5555555555555555ULL, ~std::uint64_t{0}})
        if (token.bit_scatter(token.bit_gather(value, mask), mask) != (value & mask)) return false;
    return token.masked_sum_lt_i64x4(std::span<const std::int64_t, 4>{keys, 4}, std::span<const std::int64_t, 4>{values, 4},
                                     0) == std::numeric_limits<std::int64_t>::min() &&
           token.find_first_ge_i64x4(std::span<const std::int64_t, 4>{keys, 4}, 0) == 2;
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] bool operator()(uarch::arch::X86V4 token, const std::int64_t* keys,
                                                                        const std::int64_t* values) const noexcept {
    for (auto value : std::array<std::uint64_t, 4>{0, 1, ~std::uint64_t{0}, 0x123456789abcdefULL})
      for (auto mask : std::array<std::uint64_t, 4>{0, 1ULL << 63, 0x5555555555555555ULL, ~std::uint64_t{0}})
        if (token.bit_scatter(token.bit_gather(value, mask), mask) != (value & mask)) return false;
    return token.masked_sum_lt_i64x4(std::span<const std::int64_t, 4>{keys, 4}, std::span<const std::int64_t, 4>{values, 4},
                                     0) == std::numeric_limits<std::int64_t>::min() &&
           token.find_first_ge_i64x4(std::span<const std::int64_t, 4>{keys, 4}, 0) == 2;
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] bool operator()(uarch::arch::Aarch64Neon token, const std::int64_t* keys,
                                         const std::int64_t* values) const noexcept {
    for (auto value : std::array<std::uint64_t, 4>{0, 1, ~std::uint64_t{0}, 0x123456789abcdefULL})
      for (auto mask : std::array<std::uint64_t, 4>{0, 1ULL << 63, 0x5555555555555555ULL, ~std::uint64_t{0}})
        if (token.bit_scatter(token.bit_gather(value, mask), mask) != (value & mask)) return false;
    return token.masked_sum_lt_i64x4(std::span<const std::int64_t, 4>{keys, 4}, std::span<const std::int64_t, 4>{values, 4},
                                     0) == std::numeric_limits<std::int64_t>::min() &&
           token.find_first_ge_i64x4(std::span<const std::int64_t, 4>{keys, 4}, 0) == 2;
  }
#endif
};

int main() {
  using namespace uarch::detect;
  auto before = affinity_cpus();
  auto tier = detect({WidePolicy::Allow512});
  if (!tier) return 1;
  const std::array<std::int64_t, 4> keys{std::numeric_limits<std::int64_t>::min(), -1, 0,
                                         std::numeric_limits<std::int64_t>::max()};
  const std::array<std::int64_t, 4> values{std::numeric_limits<std::int64_t>::max(), 1, 9, 37};
  bool valid = uarch::arch::dispatch_kernel<Check{}>(*tier, keys.data(), values.data());
  if (!valid) return 2;
  auto strict = detect({WidePolicy::Max256, UnknownPdep::Conservative, ExecutionDomainPolicy::RequireHomogeneous});
  auto domain = execution_domain();
#if defined(__x86_64__)
  bool homogeneous = domain && !domain->sources.empty();
  if (domain)
    for (const auto& source : domain->sources) homogeneous &= same_isa(domain->sources.front(), source);
  if (strict.has_value() != homogeneous) return 3;
#endif
  auto prefetch = detect_prefetch_write();
  if (prefetch) {
    alignas(128) std::array<char, 128> owned{};
    uarch::arch::prefetch_write(*prefetch, owned);
  } else if (prefetch.error() != DetectError::Unsupported && prefetch.error() != DetectError::UnverifiedExecutionDomain)
    return 4;
  std::error_code first = DetectError::UnverifiedExecutionDomain, second = AuditError::Malformed;
  if (!first || !second || first.category().name() != std::string_view{"uarch.detect"}) return 5;
  auto after = affinity_cpus();
  if (before.has_value() != after.has_value() || (before && *before != *after)) return 6;
  return 0;
}
