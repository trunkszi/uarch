#include <uarch/arch/tier.hpp>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/mem/pool_index.hpp>
#include <uarch/probes/gate.hpp>
using namespace uarch::probes;
extern "C" [[gnu::noinline]] uarch::tiers::TierId register_tier(uarch::arch::Tier tier) { return tier.id(); }
extern "C" [[gnu::noinline]] RegisterPair register_pair(RegisterPair pair) { return {pair.a + 1, pair.b ^ 0x55}; }
extern "C" [[gnu::noinline]] RegisterSmall register_small(RegisterSmall value) { return {value.value + 1}; }
extern "C" [[gnu::noinline]] RegisterCAbi register_c_abi(RegisterCAbi value) { return {value.a + 1, value.b ^ 0x55}; }
extern "C" [[gnu::noinline]] std::uint32_t pool_index_slot(uarch::mem::PoolIndex<int> index) { return index.slot(); }
extern "C" [[gnu::noinline]] RegisterPair register_pressure_control(std::uint64_t a, std::uint64_t b, std::uint64_t c,
                                                                    std::uint64_t d, std::uint64_t e, std::uint64_t f,
                                                                    RegisterPair pair) {
  return {pair.a + a + b + c, pair.b + d + e + f};
}
extern "C" [[gnu::noinline]] std::uint64_t memory_control_non_trivial_dtor(NonTrivialDtor8 value) {
  value.value += 1;
  return value.value - 1;
}
extern "C" [[gnu::noinline]] std::uint64_t memory_control_cache_aligned(uarch::layout::CacheAligned<std::uint64_t> value) {
  return value.value;
}

#include <uarch/core/text.hpp>
extern "C" [[gnu::noinline]] bool text_eq16(uarch::text::FixedAscii<16> a, uarch::text::FixedAscii<16> b) { return a == b; }
extern "C" [[gnu::noinline]] int text_cmp16(uarch::text::FixedAscii<16> a, uarch::text::FixedAscii<16> b) {
  const auto order = a <=> b;
  return static_cast<int>(order > 0) | -static_cast<int>(order < 0);
}
extern "C" [[gnu::noinline]] std::uint64_t hash_fnv1a64_16(uarch::text::FixedAscii<16> value) {
  return uarch::hash::fnv1a64(std::string_view{value.bytes.data(), 16});
}
