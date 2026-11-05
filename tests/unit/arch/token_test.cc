#include <csignal>
#include <uarch/arch/token.hpp>
using namespace uarch::arch;
template <Arch A>
constexpr bool sealed() {
  return !std::is_trivially_copyable_v<A> && std::is_copy_constructible_v<A> && std::is_trivially_move_constructible_v<A> &&
         std::is_trivially_copy_constructible_v<A> && std::is_trivially_destructible_v<A> &&
         uarch::layout::ZeroRegisterCapability<A> && !uarch::layout::NoPadding<A>;
}
static_assert(sealed<Scalar>() && sealed<X86V3>() && sealed<X86V3SlowPdep>() && sealed<X86V4>() && sealed<Aarch64Neon>());
static_assert(std::is_default_constructible_v<Scalar>);
static_assert(!std::is_default_constructible_v<X86V3> && !std::is_default_constructible_v<X86V3SlowPdep> &&
              !std::is_default_constructible_v<X86V4> && !std::is_default_constructible_v<Aarch64Neon>);
static_assert(!Arch<int>);
static_assert(!X86V3SlowPdep::fast_pdep && X86V3::fast_pdep);
static_assert(X86V4::vector_bytes == 64 && Aarch64Neon::max_vector_bits == 128);
static_assert(!std::is_trivially_copyable_v<PrefetchWrite>);
template <class Factory>
concept PublicMint = requires { Factory::template make<X86V3>(); };
template <class Factory>
concept PublicTierMint = requires { Factory::make_tier(uarch::tiers::TierId::X86V3); };
// A reflected public issuer type must not expose issuance through an alias.
consteval std::meta::info issuer_type() {
  for (auto member : std::meta::members_of(^^uarch::arch::detail, std::meta::access_context::current()))
    if (std::meta::is_type(member) && std::meta::has_identifier(member) && std::meta::identifier_of(member) == "TokenFactory")
      return member;
  std::abort();
}
using Issuer = [:issuer_type():];
static_assert(!PublicMint<Issuer>);
static_assert(!PublicTierMint<Issuer>);
extern "C" [[gnu::noinline]] std::uint64_t empty_capability_payload(X86V3 token, std::uint64_t a, std::uint64_t b,
                                                                    std::uint64_t c, std::uint64_t d, std::uint64_t e,
                                                                    std::uint64_t f) {
  static_cast<void>(token == token);
  return a + b + c + d + e + f;
}
int main() {
  auto a = X86V3::assume_supported(unchecked_tag);
  volatile std::uint64_t payload[] = {1, 2, 3, 4, 5, 6};
  if (empty_capability_payload(a, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]) != 21) return 1;
  [[maybe_unused]] auto b = retag_unchecked<X86V3>(a);
  if (std::signal(SIGABRT, +[](int) { std::_Exit(0); }) == SIG_ERR) return 1;
  [[maybe_unused]] auto invalid = retag_unchecked<X86V4>(a);
  return 1;
}
