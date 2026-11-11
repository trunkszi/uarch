/** @file @brief Cold call boundaries preserving audited argument passing. */
#pragma once
#include <uarch/core/flow/select.hpp>
#include <uarch/core/layout/register_passable.hpp>
namespace uarch::flow {
[[gnu::cold, gnu::noinline]] inline constexpr void cold_path() noexcept {}
constexpr bool likely(bool b) noexcept { return __builtin_expect(b, true); }
constexpr bool unlikely(bool b) noexcept { return __builtin_expect(b, false); }
template <auto Fn, RegisterSelectable... Args>
[[gnu::cold, gnu::noinline]] constexpr decltype(auto) cold_call(Args... args) noexcept(noexcept(Fn(args...))) {
  return Fn(args...);
}
template <class T>
  requires std::is_object_v<std::remove_cvref_t<T>>
using ColdArg = std::conditional_t<layout::RegisterPassable<std::remove_cvref_t<T>> ||
                                       layout::ZeroRegisterCapability<std::remove_cvref_t<T>>,
                                   std::remove_cvref_t<T>, const std::remove_cvref_t<T>&>;
namespace detail {
template <auto Fn, class... Params>
[[gnu::cold, gnu::noinline]] constexpr decltype(auto) cold_trampoline(Params... params) noexcept(noexcept(Fn(params...))) {
  return Fn(params...);
}
}  // namespace detail
template <auto Fn, class... Args>
[[gnu::always_inline]] constexpr decltype(auto) cold_invoke(Args&&... args) noexcept(noexcept(Fn(args...))) {
  return detail::cold_trampoline<Fn, ColdArg<Args>...>(args...);
}
}  // namespace uarch::flow
