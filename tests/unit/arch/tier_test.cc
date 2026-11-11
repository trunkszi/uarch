#include <uarch/arch/tier.hpp>
using namespace uarch::arch;
static_assert(uarch::layout::RegisterPassable<Tier>);
static_assert(!std::is_default_constructible_v<Tier> && !std::is_trivially_copyable_v<Tier>);
constexpr uarch::arch::Tier mint(uarch::tiers::TierId id) {
  using namespace uarch::arch;
  switch (id) {
    case uarch::tiers::TierId::Scalar:
      return Tier(Scalar{});
    case uarch::tiers::TierId::X86V3:
      return Tier(X86V3::assume_supported(unchecked_tag));
    case uarch::tiers::TierId::X86V3SlowPdep:
      return Tier(X86V3SlowPdep::assume_supported(unchecked_tag));
    case uarch::tiers::TierId::X86V4:
      return Tier(X86V4::assume_supported(unchecked_tag));
    case uarch::tiers::TierId::Aarch64Neon:
      return Tier(Aarch64Neon::assume_supported(unchecked_tag));
  }
  std::abort();
}
constexpr bool check() {
  for (auto id : uarch::tiers::detail::kTierIds) {
    auto tier = mint(id);
    auto found = tier.dispatch([]<Arch A>(A) { return A::tier; });
    if (found != id) return false;
  }
  return true;
}
static_assert(check());
int main() { return !check(); }
