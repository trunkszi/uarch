#pragma once
#include <uarch/core/flow/select.hpp>
namespace uarch::flow {
[[gnu::cold, gnu::noinline]] inline constexpr void cold_path() noexcept {}
constexpr bool likely(bool b) noexcept { return __builtin_expect(b, true); }
constexpr bool unlikely(bool b) noexcept { return __builtin_expect(b, false); }
template <auto Fn, RegisterSelectable... Args>
[[gnu::cold, gnu::noinline]] constexpr decltype(auto) cold_call(Args... args) noexcept(noexcept(Fn(args...))) {
  return Fn(args...);
}
}  // namespace uarch::flow
