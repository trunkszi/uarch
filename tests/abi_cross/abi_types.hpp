#pragma once
#include <uarch/core/arithmetic/fast_divisor_u32.hpp>
#include <uarch/core/bits/bit_word.hpp>
#include <uarch/core/flow/predicate.hpp>
#include <uarch/core/mem/pool_index.hpp>
#include <uarch/probes/gate.hpp>
namespace uarch::cross {
using probes::RegisterCAbi;
using probes::RegisterPair;
using probes::RegisterSmall;
struct Spec {
  using Word = std::uint64_t;
  std::uint64_t value[[= bits::Field{0, 64}]];
};
using Word = bits::BitWord<Spec>;
RegisterPair pair(RegisterPair);
RegisterSmall small(RegisterSmall);
RegisterCAbi c_abi(RegisterCAbi);
mem::PoolIndex<int> index(mem::PoolIndex<int>);
mem::OptionalPoolIndex<int> optional_index(mem::OptionalPoolIndex<int>);
flow::Predicate predicate(flow::Predicate);
Word word(Word);
arithmetic::FastDivisorU32 divisor(arithmetic::FastDivisorU32);
static_assert(layout::RegisterPassable<mem::PoolIndex<int>> && layout::RegisterPassable<mem::OptionalPoolIndex<int>>);
static_assert(layout::RegisterPassable<flow::Predicate> && layout::RegisterPassable<Word> &&
              layout::RegisterPassable<arithmetic::FastDivisorU32>);
}  // namespace uarch::cross
