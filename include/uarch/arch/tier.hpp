#pragma once
#include <uarch/arch/token.hpp>
#include <utility>
namespace uarch::arch {
namespace detail {
template <tiers::TierId>
struct TokenOf;
template <>
struct TokenOf<tiers::TierId::Scalar> {
  using type = Scalar;
};
template <>
struct TokenOf<tiers::TierId::X86V3> {
  using type = X86V3;
};
template <>
struct TokenOf<tiers::TierId::X86V3SlowPdep> {
  using type = X86V3SlowPdep;
};
template <>
struct TokenOf<tiers::TierId::X86V4> {
  using type = X86V4;
};
template <>
struct TokenOf<tiers::TierId::Aarch64Neon> {
  using type = Aarch64Neon;
};
}  // namespace detail
/// A minted tier is permission, whereas a descriptive TierId alone is not.
class Tier {
  tiers::TierId id_;
  friend class detail::TokenFactory;
  constexpr explicit Tier(tiers::TierId id) noexcept : id_(id) {
    if (!tiers::meta_of(id)) std::abort();
  }

 public:
  Tier() = delete ("construct Tier from a proven capability token");
  template <Arch A>
  constexpr explicit Tier(A) noexcept : id_(A::tier) {}
  constexpr Tier(const Tier&) noexcept = default;
  constexpr Tier(Tier&&) noexcept = default;
  constexpr Tier& operator=(const Tier& other) noexcept {
    if (this != &other) id_ = other.id_;
    return *this;
  }
  constexpr Tier& operator=(Tier&& other) noexcept {
    if (this != &other) id_ = other.id_;
    return *this;
  }
  constexpr ~Tier() noexcept = default;
  [[nodiscard, gnu::always_inline]] constexpr tiers::TierId id() const noexcept { return id_; }
  template <class Visitor>
  [[gnu::always_inline]] constexpr decltype(auto) dispatch(this Tier self, Visitor&& visitor) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^tiers::TierId))) {
      if (self.id_ == [:e:])
        return std::forward<Visitor>(visitor)(detail::TokenFactory::make<typename detail::TokenOf<([:e:])>::type>());
    }
    std::abort();
  }
};
static_assert(!std::is_trivially_copyable_v<Tier> && layout::RegisterPassable<Tier>);
constexpr Tier detail::TokenFactory::make_tier(tiers::TierId id) noexcept { return Tier(id); }
}  // namespace uarch::arch
