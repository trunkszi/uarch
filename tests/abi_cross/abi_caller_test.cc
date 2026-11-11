#include "abi_types.hpp"
int main() {
  using namespace uarch;
  for (std::uint64_t n : {0ULL, 1ULL, 0x8000000000000000ULL, 0xffffffffffffffffULL}) {
    auto p = cross::pair({n, ~n});
    if (p.a != (n ^ 0x123456789abcdef0ULL) || p.b != ~n + 7) return 1;
    auto s = cross::small({static_cast<std::uint32_t>(n)});
    if (s.value != (static_cast<std::uint32_t>(n) ^ 0xa5a55a5aU)) return 1;
    auto c = cross::c_abi({static_cast<std::uint32_t>(n), static_cast<std::uint32_t>(~n)});
    if (c.a != static_cast<std::uint32_t>(~n) || c.b != static_cast<std::uint32_t>(n)) return 1;
    auto w = cross::word(cross::Word::from_raw(n));
    if (w.raw() != (n ^ 0xa55aa55aa55aa55aULL)) return 1;
  }
  for (std::uint32_t slot : {1U, 7U, 0x80000000U, UINT32_MAX}) {
    auto i = *mem::PoolIndex<int>::try_new(slot);
    if (cross::index(i).slot() != slot) return 1;
    auto opt = cross::optional_index(mem::OptionalPoolIndex<int>::some(i));
    if (!opt.get() || opt.get()->slot() != slot) return 1;
  }
  if (cross::optional_index(mem::OptionalPoolIndex<int>::none()).has_value()) return 1;
  for (bool b : {false, true})
    if (cross::predicate(flow::predicate(b)).as_bool() == b) return 1;
  for (std::uint32_t d : {1U, 2U, 3U, 7U, UINT32_MAX}) {
    auto div = cross::divisor(*arithmetic::FastDivisorU32::try_new(d));
    if (div.divisor() != d || div.div(UINT32_MAX) != UINT32_MAX / d || div.rem(UINT32_MAX) != UINT32_MAX % d) return 1;
  }
}
