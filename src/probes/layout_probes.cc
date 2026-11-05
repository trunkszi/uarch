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
