#pragma once
#include <cstdint>
#include <span>
#include <uarch/core/mem/pool_index.hpp>
namespace uarch::probes {
using DelegateSpan = std::span<const int>;
using DelegateIndexValue = mem::PoolIndex<struct DelegateIndex>;
struct DelegatePrefix {
  std::uint64_t padding{};
};
struct DelegateTarget {
  std::uint64_t bias{7};
  [[gnu::noinline]] int integer(int x) const noexcept;
  [[gnu::noinline]] int span(DelegateSpan x) const noexcept;
  [[gnu::noinline]] int index(DelegateIndexValue x) const noexcept;
};
struct DelegateNonprimary : DelegatePrefix, DelegateTarget {};
}  // namespace uarch::probes
