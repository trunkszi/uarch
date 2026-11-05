#include "abi_types.hpp"
namespace uarch::cross {
[[gnu::noinline]] RegisterPair pair(RegisterPair x) { return {x.a ^ 0x123456789abcdef0ULL, x.b + 7}; }
[[gnu::noinline]] RegisterSmall small(RegisterSmall x) { return {x.value ^ 0xa5a55a5aU}; }
[[gnu::noinline]] RegisterCAbi c_abi(RegisterCAbi x) { return {x.b, x.a}; }
[[gnu::noinline]] mem::PoolIndex<int> index(mem::PoolIndex<int> x) { return x; }
[[gnu::noinline]] mem::OptionalPoolIndex<int> optional_index(mem::OptionalPoolIndex<int> x) { return x; }
[[gnu::noinline]] flow::Predicate predicate(flow::Predicate x) { return !x; }
[[gnu::noinline]] Word word(Word x) { return Word::from_raw(x.raw() ^ 0xa55aa55aa55aa55aULL); }
[[gnu::noinline]] arithmetic::FastDivisorU32 divisor(arithmetic::FastDivisorU32 x) { return x; }
}  // namespace uarch::cross
