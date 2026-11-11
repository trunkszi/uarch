#pragma once
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <uarch/arch/tier.hpp>
namespace uarch::arch {
namespace detail {
template <class T>
concept DirectEntryValue = std::is_void_v<T> || (sizeof(std::remove_cvref_t<T>) <= 16 && alignof(std::remove_cvref_t<T>) <= 16);
template <class T>
using EntryParam = std::conditional_t<DirectEntryValue<T>, std::remove_cvref_t<T>, const std::remove_cvref_t<T>&>;
template <auto Body, class Token, class... Args>
using EntryReturn = std::remove_cvref_t<std::invoke_result_t<decltype(Body), Token, Args...>>;
template <auto Body, class Token, class... Args>
concept DirectEntryCall = (DirectEntryValue<Args> && ...) && DirectEntryValue<EntryReturn<Body, Token, Args...>>;

// The output object starts in the target thunk; wide values never use its return convention.
template <class R, class Invoke>
[[gnu::always_inline]] inline auto entry_result(Invoke invoke) {
  union Storage {
    R value;
    Storage() noexcept {}
    ~Storage() noexcept {}
  } storage;
  auto* value = std::addressof(storage.value);
  invoke(value);
  struct Cleanup {
    R* value;
    ~Cleanup() noexcept { std::destroy_at(value); }
  } cleanup{value};
  return std::move(storage.value);
}
}  // namespace detail

/// ISA-using Body overloads must declare the same literal target as their entry.
/// Narrow values cross directly; wide values use references and explicit return storage.
/// GCC entry thunks disable IPA so scalar replacement cannot reintroduce a target-dependent vector ABI.
template <auto Body, class... Args>
[[gnu::always_inline]] constexpr auto run_scalar(Scalar token, Args... args) noexcept {
  return Body(token, args...);
}
#if defined(__x86_64__)
namespace detail {
template <auto Body, class... Args>
  requires DirectEntryValue<EntryReturn<Body, X86V3, Args...>>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] auto enter_x86_v3(X86V3 token, EntryParam<Args>... args) noexcept
    -> EntryReturn<Body, X86V3, Args...> {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!DirectEntryValue<EntryReturn<Body, X86V3, Args...>>)
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] void enter_x86_v3_out(EntryReturn<Body, X86V3, Args...>* out, X86V3 token,
                                                                       EntryParam<Args>... args) noexcept {
  using R = EntryReturn<Body, X86V3, Args...>;
  ::new (static_cast<void*>(out)) R(Body(token, args...));
}
}  // namespace detail
template <auto Body, class... Args>
  requires detail::DirectEntryCall<Body, X86V3, Args...>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] auto run_x86_v3(X86V3 token, Args... args) noexcept {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!detail::DirectEntryCall<Body, X86V3, Args...>)
[[gnu::always_inline]] inline auto run_x86_v3(X86V3 token, Args... args) noexcept {
  using R = detail::EntryReturn<Body, X86V3, Args...>;
  if constexpr (detail::DirectEntryValue<R>) {
    return detail::enter_x86_v3<Body, Args...>(token, args...);
  } else {
    return detail::entry_result<R>([&](R* out) { detail::enter_x86_v3_out<Body, Args...>(out, token, args...); });
  }
}
#endif
#if defined(__x86_64__)
namespace detail {
template <auto Body, class... Args>
  requires DirectEntryValue<EntryReturn<Body, X86V3SlowPdep, Args...>>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] auto enter_x86_v3_slow_pdep(X86V3SlowPdep token,
                                                                             EntryParam<Args>... args) noexcept
    -> EntryReturn<Body, X86V3SlowPdep, Args...> {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!DirectEntryValue<EntryReturn<Body, X86V3SlowPdep, Args...>>)
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] void enter_x86_v3_slow_pdep_out(EntryReturn<Body, X86V3SlowPdep, Args...>* out,
                                                                                 X86V3SlowPdep token,
                                                                                 EntryParam<Args>... args) noexcept {
  using R = EntryReturn<Body, X86V3SlowPdep, Args...>;
  ::new (static_cast<void*>(out)) R(Body(token, args...));
}
}  // namespace detail
template <auto Body, class... Args>
  requires detail::DirectEntryCall<Body, X86V3SlowPdep, Args...>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v3"), gnu::noinline]] auto run_x86_v3_slow_pdep(X86V3SlowPdep token, Args... args) noexcept {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!detail::DirectEntryCall<Body, X86V3SlowPdep, Args...>)
[[gnu::always_inline]] inline auto run_x86_v3_slow_pdep(X86V3SlowPdep token, Args... args) noexcept {
  using R = detail::EntryReturn<Body, X86V3SlowPdep, Args...>;
  if constexpr (detail::DirectEntryValue<R>) {
    return detail::enter_x86_v3_slow_pdep<Body, Args...>(token, args...);
  } else {
    return detail::entry_result<R>([&](R* out) { detail::enter_x86_v3_slow_pdep_out<Body, Args...>(out, token, args...); });
  }
}
#endif
#if defined(__x86_64__)
namespace detail {
template <auto Body, class... Args>
  requires DirectEntryValue<EntryReturn<Body, X86V4, Args...>>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v4"), gnu::noinline]] auto enter_x86_v4(X86V4 token, EntryParam<Args>... args) noexcept
    -> EntryReturn<Body, X86V4, Args...> {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!DirectEntryValue<EntryReturn<Body, X86V4, Args...>>)
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v4"), gnu::noinline]] void enter_x86_v4_out(EntryReturn<Body, X86V4, Args...>* out, X86V4 token,
                                                                       EntryParam<Args>... args) noexcept {
  using R = EntryReturn<Body, X86V4, Args...>;
  ::new (static_cast<void*>(out)) R(Body(token, args...));
}
}  // namespace detail
template <auto Body, class... Args>
  requires detail::DirectEntryCall<Body, X86V4, Args...>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::target("arch=x86-64-v4"), gnu::noinline]] auto run_x86_v4(X86V4 token, Args... args) noexcept {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!detail::DirectEntryCall<Body, X86V4, Args...>)
[[gnu::always_inline]] inline auto run_x86_v4(X86V4 token, Args... args) noexcept {
  using R = detail::EntryReturn<Body, X86V4, Args...>;
  if constexpr (detail::DirectEntryValue<R>) {
    return detail::enter_x86_v4<Body, Args...>(token, args...);
  } else {
    return detail::entry_result<R>([&](R* out) { detail::enter_x86_v4_out<Body, Args...>(out, token, args...); });
  }
}
#endif
#if defined(__aarch64__)
namespace detail {
template <auto Body, class... Args>
  requires DirectEntryValue<EntryReturn<Body, Aarch64Neon, Args...>>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::noinline]] auto enter_aarch64_neon(Aarch64Neon token, EntryParam<Args>... args) noexcept
    -> EntryReturn<Body, Aarch64Neon, Args...> {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!DirectEntryValue<EntryReturn<Body, Aarch64Neon, Args...>>)
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::noinline]] void enter_aarch64_neon_out(EntryReturn<Body, Aarch64Neon, Args...>* out, Aarch64Neon token,
                                              EntryParam<Args>... args) noexcept {
  using R = EntryReturn<Body, Aarch64Neon, Args...>;
  ::new (static_cast<void*>(out)) R(Body(token, args...));
}
}  // namespace detail
template <auto Body, class... Args>
  requires detail::DirectEntryCall<Body, Aarch64Neon, Args...>
#if !defined(__clang__)
[[gnu::noipa]]
#endif
[[gnu::noinline]] auto run_aarch64_neon(Aarch64Neon token, Args... args) noexcept {
  return Body(token, args...);
}
template <auto Body, class... Args>
  requires(!detail::DirectEntryCall<Body, Aarch64Neon, Args...>)
[[gnu::always_inline]] inline auto run_aarch64_neon(Aarch64Neon token, Args... args) noexcept {
  using R = detail::EntryReturn<Body, Aarch64Neon, Args...>;
  if constexpr (detail::DirectEntryValue<R>) {
    return detail::enter_aarch64_neon<Body, Args...>(token, args...);
  } else {
    return detail::entry_result<R>([&](R* out) { detail::enter_aarch64_neon_out<Body, Args...>(out, token, args...); });
  }
}
#endif
template <auto Body, Arch A, class... Args>
[[gnu::always_inline]] constexpr auto run(A token, Args... args) noexcept {
  if constexpr (std::same_as<A, Scalar>) return run_scalar<Body>(token, std::move(args)...);
#if defined(__x86_64__)
  else if constexpr (std::same_as<A, X86V3>)
    return run_x86_v3<Body>(token, std::move(args)...);
  else if constexpr (std::same_as<A, X86V3SlowPdep>)
    return run_x86_v3_slow_pdep<Body>(token, std::move(args)...);
  else if constexpr (std::same_as<A, X86V4>)
    return run_x86_v4<Body>(token, std::move(args)...);
#endif
#if defined(__aarch64__)
  else if constexpr (std::same_as<A, Aarch64Neon>)
    return run_aarch64_neon<Body>(token, std::move(args)...);
#endif
  else {
    // A foreign-architecture permit cannot authorize execution on this target.
    std::abort();
    return run_scalar<Body>(Scalar{}, std::move(args)...);
  }
}
template <auto Body, class... Args>
[[gnu::always_inline]] constexpr auto dispatch_kernel(Tier tier, Args... args) {
  return tier.dispatch([&]<Arch A> [[gnu::always_inline]] (A token) { return run<Body>(token, args...); });
}
}  // namespace uarch::arch
