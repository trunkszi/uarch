#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <type_traits>
#include <uarch/arch/kernels/aarch64_neon.hpp>
#include <uarch/arch/kernels/scalar.hpp>
#include <uarch/arch/kernels/x86_v3.hpp>
#include <uarch/arch/kernels/x86_v3_slow_pdep.hpp>
#include <uarch/arch/kernels/x86_v4.hpp>
#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/mem/prefetch.hpp>
#include <uarch/tiers/tier_id.hpp>
namespace uarch::arch {
class Tier;
}
namespace uarch::detect {
struct DetectPolicy;
enum class DetectError : std::uint8_t;
[[nodiscard]] std::expected<arch::Tier, DetectError> detect(DetectPolicy) noexcept;
}  // namespace uarch::detect
namespace uarch::arch {
struct UncheckedTag {
  explicit constexpr UncheckedTag() noexcept = default;
};
inline constexpr UncheckedTag unchecked_tag{};
namespace detail {
struct MintTag {};
class TokenFactory;
}  // namespace detail
class Scalar final {
  friend class detail::TokenFactory;
  constexpr explicit Scalar(detail::MintTag) noexcept {}

 public:
  constexpr Scalar() noexcept = default;
  constexpr Scalar(const Scalar&) noexcept = default;
  constexpr Scalar(Scalar&&) noexcept = default;
  constexpr Scalar& operator=(const Scalar& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr Scalar& operator=(Scalar&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~Scalar() noexcept = default;
  constexpr bool operator==(const Scalar&) const noexcept = default;
  /// Caller proves this ISA on every CPU on which the permit can execute.
  static constexpr Scalar assume_supported(UncheckedTag = unchecked_tag) noexcept { return Scalar(detail::MintTag{}); }
  static constexpr tiers::TierId tier = tiers::TierId::Scalar;
  static constexpr auto name = *tiers::as_str(tier);
  static constexpr bool fast_pdep = *tiers::has_fast_pdep(tier);
  static constexpr auto vector_bytes = *tiers::vector_bytes(tier);
  static constexpr auto max_vector_bits = *tiers::max_vector_bits(tier);
  static constexpr std::uint64_t bit_gather_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_gather(v, m);
  }
  static constexpr std::uint64_t bit_scatter_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_scatter(v, m);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t bit_gather(std::uint64_t v, std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::scalar::bit_gather(v, m);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t bit_scatter(std::uint64_t v, std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::scalar::bit_scatter(v, m);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::int64_t masked_sum_lt_i64x4(std::span<const std::int64_t, 4> k,
                                                                               std::span<const std::int64_t, 4> v,
                                                                               std::int64_t limit) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::scalar::masked_sum_lt_i64x4(k, v, limit);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> find_first_ge_i64x4(
      std::span<const std::int64_t, 4> k, std::int64_t target) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::scalar::find_first_ge_i64x4(k, target);
  }
};
class X86V3 final {
  friend class detail::TokenFactory;
  constexpr explicit X86V3(detail::MintTag) noexcept {}

 public:
  X86V3() = delete ("capability tokens come from uarch::detect::detect() or assume_supported(UncheckedTag)");
  constexpr X86V3(const X86V3&) noexcept = default;
  constexpr X86V3(X86V3&&) noexcept = default;
  constexpr X86V3& operator=(const X86V3& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr X86V3& operator=(X86V3&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~X86V3() noexcept = default;
  constexpr bool operator==(const X86V3&) const noexcept = default;
  /// Caller proves this ISA on every CPU on which the permit can execute.
  static constexpr X86V3 assume_supported(UncheckedTag) noexcept { return X86V3(detail::MintTag{}); }
  static constexpr tiers::TierId tier = tiers::TierId::X86V3;
  static constexpr auto name = *tiers::as_str(tier);
  static constexpr bool fast_pdep = *tiers::has_fast_pdep(tier);
  static constexpr auto vector_bytes = *tiers::vector_bytes(tier);
  static constexpr auto max_vector_bits = *tiers::max_vector_bits(tier);
  static constexpr std::uint64_t bit_gather_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_gather(v, m);
  }
  static constexpr std::uint64_t bit_scatter_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_scatter(v, m);
  }
#if defined(__x86_64__)
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t bit_gather(std::uint64_t v,
                                                                                            std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3::bit_gather(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t bit_scatter(std::uint64_t v,
                                                                                             std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3::bit_scatter(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::int64_t masked_sum_lt_i64x4(
      std::span<const std::int64_t, 4> k, std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3::masked_sum_lt_i64x4(k, v, limit);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::optional<std::uint32_t> find_first_ge_i64x4(
      std::span<const std::int64_t, 4> k, std::int64_t target) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3::find_first_ge_i64x4(k, target);
  }
#endif
};
class X86V3SlowPdep final {
  friend class detail::TokenFactory;
  constexpr explicit X86V3SlowPdep(detail::MintTag) noexcept {}

 public:
  X86V3SlowPdep() = delete ("capability tokens come from uarch::detect::detect() or assume_supported(UncheckedTag)");
  constexpr X86V3SlowPdep(const X86V3SlowPdep&) noexcept = default;
  constexpr X86V3SlowPdep(X86V3SlowPdep&&) noexcept = default;
  constexpr X86V3SlowPdep& operator=(const X86V3SlowPdep& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr X86V3SlowPdep& operator=(X86V3SlowPdep&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~X86V3SlowPdep() noexcept = default;
  constexpr bool operator==(const X86V3SlowPdep&) const noexcept = default;
  /// Caller proves this ISA on every CPU on which the permit can execute.
  static constexpr X86V3SlowPdep assume_supported(UncheckedTag) noexcept { return X86V3SlowPdep(detail::MintTag{}); }
  static constexpr tiers::TierId tier = tiers::TierId::X86V3SlowPdep;
  static constexpr auto name = *tiers::as_str(tier);
  static constexpr bool fast_pdep = *tiers::has_fast_pdep(tier);
  static constexpr auto vector_bytes = *tiers::vector_bytes(tier);
  static constexpr auto max_vector_bits = *tiers::max_vector_bits(tier);
  static constexpr std::uint64_t bit_gather_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_gather(v, m);
  }
  static constexpr std::uint64_t bit_scatter_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_scatter(v, m);
  }
#if defined(__x86_64__)
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t bit_gather(std::uint64_t v,
                                                                                            std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3_slow_pdep::bit_gather(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t bit_scatter(std::uint64_t v,
                                                                                             std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3_slow_pdep::bit_scatter(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::int64_t masked_sum_lt_i64x4(
      std::span<const std::int64_t, 4> k, std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3_slow_pdep::masked_sum_lt_i64x4(k, v, limit);
  }
  [[nodiscard, gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::optional<std::uint32_t> find_first_ge_i64x4(
      std::span<const std::int64_t, 4> k, std::int64_t target) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v3_slow_pdep::find_first_ge_i64x4(k, target);
  }
#endif
};
class X86V4 final {
  friend class detail::TokenFactory;
  constexpr explicit X86V4(detail::MintTag) noexcept {}

 public:
  X86V4() = delete ("capability tokens come from uarch::detect::detect() or assume_supported(UncheckedTag)");
  constexpr X86V4(const X86V4&) noexcept = default;
  constexpr X86V4(X86V4&&) noexcept = default;
  constexpr X86V4& operator=(const X86V4& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr X86V4& operator=(X86V4&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~X86V4() noexcept = default;
  constexpr bool operator==(const X86V4&) const noexcept = default;
  /// Caller proves this ISA on every CPU on which the permit can execute.
  static constexpr X86V4 assume_supported(UncheckedTag) noexcept { return X86V4(detail::MintTag{}); }
  static constexpr tiers::TierId tier = tiers::TierId::X86V4;
  static constexpr auto name = *tiers::as_str(tier);
  static constexpr bool fast_pdep = *tiers::has_fast_pdep(tier);
  static constexpr auto vector_bytes = *tiers::vector_bytes(tier);
  static constexpr auto max_vector_bits = *tiers::max_vector_bits(tier);
  static constexpr std::uint64_t bit_gather_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_gather(v, m);
  }
  static constexpr std::uint64_t bit_scatter_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_scatter(v, m);
  }
#if defined(__x86_64__)
  [[nodiscard, gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::uint64_t bit_gather(std::uint64_t v,
                                                                                            std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v4::bit_gather(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::uint64_t bit_scatter(std::uint64_t v,
                                                                                             std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v4::bit_scatter(v, m);
  }
  [[nodiscard, gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::int64_t masked_sum_lt_i64x4(
      std::span<const std::int64_t, 4> k, std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v4::masked_sum_lt_i64x4(k, v, limit);
  }
  [[nodiscard, gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::optional<std::uint32_t> find_first_ge_i64x4(
      std::span<const std::int64_t, 4> k, std::int64_t target) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::x86_v4::find_first_ge_i64x4(k, target);
  }
#endif
};
class Aarch64Neon final {
  friend class detail::TokenFactory;
  constexpr explicit Aarch64Neon(detail::MintTag) noexcept {}

 public:
  Aarch64Neon() = delete ("capability tokens come from uarch::detect::detect() or assume_supported(UncheckedTag)");
  constexpr Aarch64Neon(const Aarch64Neon&) noexcept = default;
  constexpr Aarch64Neon(Aarch64Neon&&) noexcept = default;
  constexpr Aarch64Neon& operator=(const Aarch64Neon& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr Aarch64Neon& operator=(Aarch64Neon&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~Aarch64Neon() noexcept = default;
  constexpr bool operator==(const Aarch64Neon&) const noexcept = default;
  /// Caller proves this ISA on every CPU on which the permit can execute.
  static constexpr Aarch64Neon assume_supported(UncheckedTag) noexcept { return Aarch64Neon(detail::MintTag{}); }
  static constexpr tiers::TierId tier = tiers::TierId::Aarch64Neon;
  static constexpr auto name = *tiers::as_str(tier);
  static constexpr bool fast_pdep = *tiers::has_fast_pdep(tier);
  static constexpr auto vector_bytes = *tiers::vector_bytes(tier);
  static constexpr auto max_vector_bits = *tiers::max_vector_bits(tier);
  static constexpr std::uint64_t bit_gather_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_gather(v, m);
  }
  static constexpr std::uint64_t bit_scatter_constexpr(std::uint64_t v, std::uint64_t m) noexcept {
    return kernels::scalar::bit_scatter(v, m);
  }
#if defined(__aarch64__)
  [[nodiscard, gnu::always_inline]] std::uint64_t bit_gather(std::uint64_t v, std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::aarch64_neon::bit_gather(v, m);
  }
  [[nodiscard, gnu::always_inline]] std::uint64_t bit_scatter(std::uint64_t v, std::uint64_t m) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::aarch64_neon::bit_scatter(v, m);
  }
  [[nodiscard, gnu::always_inline]] std::int64_t masked_sum_lt_i64x4(std::span<const std::int64_t, 4> k,
                                                                     std::span<const std::int64_t, 4> v,
                                                                     std::int64_t limit) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::aarch64_neon::masked_sum_lt_i64x4(k, v, limit);
  }
  [[nodiscard, gnu::always_inline]] std::optional<std::uint32_t> find_first_ge_i64x4(std::span<const std::int64_t, 4> k,
                                                                                     std::int64_t target) const noexcept {
    static_cast<void>(*this == *this);
    return kernels::aarch64_neon::find_first_ge_i64x4(k, target);
  }
#endif
};
static_assert(layout::ZeroRegisterCapability<Scalar> && layout::ZeroRegisterCapability<X86V3> &&
              layout::ZeroRegisterCapability<X86V3SlowPdep> && layout::ZeroRegisterCapability<X86V4> &&
              layout::ZeroRegisterCapability<Aarch64Neon>);

template <class A>
concept Arch = std::same_as<std::remove_cvref_t<A>, Scalar> || std::same_as<std::remove_cvref_t<A>, X86V3> ||
               std::same_as<std::remove_cvref_t<A>, X86V3SlowPdep> || std::same_as<std::remove_cvref_t<A>, X86V4> ||
               std::same_as<std::remove_cvref_t<A>, Aarch64Neon>;
class Tier;
template <Arch To, Arch From>
constexpr To retag_unchecked(From, UncheckedTag = unchecked_tag) noexcept;
namespace detail {
class TokenFactory {
  friend class uarch::arch::Tier;
  template <Arch To, Arch From>
  friend constexpr To uarch::arch::retag_unchecked(From, UncheckedTag) noexcept;
  friend std::expected<uarch::arch::Tier, uarch::detect::DetectError> uarch::detect::detect(
      uarch::detect::DetectPolicy) noexcept;
  template <Arch A>
  static constexpr A make() noexcept {
    return A(MintTag{});
  }
  static constexpr Tier make_tier(tiers::TierId id) noexcept;
};
}  // namespace detail
template <Arch To, Arch From>
constexpr To retag_unchecked(From, UncheckedTag) noexcept {
  if (From::tier != To::tier) std::abort();
  return detail::TokenFactory::make<To>();
}
using mem::prefetch_read;
using mem::prefetch_write;
using mem::PrefetchWrite;
}  // namespace uarch::arch
