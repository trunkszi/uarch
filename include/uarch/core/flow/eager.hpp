#pragma once
#include <concepts>
namespace uarch::flow {
template <class... Bools>
  requires(std::convertible_to<Bools, bool> && ...)
constexpr bool eager_all(Bools... bs) noexcept {
  return ((1U & ... & static_cast<unsigned>(static_cast<bool>(bs))) != 0);
}
template <class... Bools>
  requires(std::convertible_to<Bools, bool> && ...)
constexpr bool eager_any(Bools... bs) noexcept {
  return ((0U | ... | static_cast<unsigned>(static_cast<bool>(bs))) != 0);
}
}  // namespace uarch::flow
