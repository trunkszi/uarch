/** @file @brief Borrowed compile-time targets with audited register or consuming-reference transport. */
#pragma once
#include <type_traits>
#include <uarch/core/layout/register_passable.hpp>
#include <utility>

namespace uarch::flow {
namespace detail {
template <class Pointer>
constexpr bool callback_present(Pointer stub) noexcept {
  if consteval {
    // GCC UBSan cannot evaluate direct function-pointer comparisons; constant identity preserves the same test.
    return std::meta::reflect_constant(stub) != std::meta::reflect_constant(Pointer{});
  } else {
    return stub != nullptr;
  }
}

template <class T>
concept PointerReceiver = std::is_pointer_v<T> || (requires {
                            typename std::remove_cv_t<T>::element_type;
                          } && (requires(T& object) { object.get(); } || requires(T& object) { object.lock(); }));
template <class T>
consteval bool pass_by_value_subobject(std::size_t offset = 0) noexcept {
  using U = std::remove_cv_t<T>;
  if constexpr (sizeof(U) > 16 || alignof(U) > 16 || std::is_same_v<U, long double>) {
    return false;
  } else if constexpr (std::is_bounded_array_v<U>) {
    // Arrays have no copy constructor of their own; their leaves carry the call-trivial proof.
    using M = std::remove_extent_t<U>;
    for (std::size_t i = 0; i < std::extent_v<U>; ++i)
      if (!pass_by_value_subobject<M>(offset + i * sizeof(M))) return false;
    return true;
  } else if constexpr (!layout::CallTrivial<U>) {
    return false;
  } else if constexpr (std::is_empty_v<U>) {
    return true;
  } else if constexpr (std::is_integral_v<U> || std::is_pointer_v<U> || std::is_enum_v<U> || std::is_member_pointer_v<U> ||
                       std::is_same_v<U, decltype(nullptr)> || std::is_same_v<U, __int128_t> ||
                       std::is_same_v<U, __uint128_t> || std::is_same_v<U, float> || std::is_same_v<U, double>) {
    if (offset % alignof(U) != 0 || offset + sizeof(U) > 16) return false;
    if constexpr (sizeof(U) > 8)
      return offset == 0;
    else
      return (offset / 8) == ((offset + sizeof(U) - 1) / 8);
  } else if constexpr (std::is_union_v<U>) {
    constexpr auto ctx = reflect::unchecked_context();
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^U, ctx))) {
      if constexpr (std::meta::is_bit_field(m)) {
        if (static_cast<std::size_t>(std::meta::bit_size_of(m)) > 64) return false;
      } else {
        using M = [:std::meta::type_of(m):];
        if constexpr (!std::is_empty_v<M>) {
          if (!pass_by_value_subobject<M>(offset)) return false;
        }
      }
    }
    return true;
  } else if constexpr (std::is_class_v<U>) {
    constexpr auto ctx = reflect::unchecked_context();
    template for (constexpr auto b : std::define_static_array(std::meta::bases_of(^^U, ctx))) {
      if constexpr (std::meta::is_virtual(b)) return false;
      using B = [:std::meta::type_of(b):];
      if constexpr (!std::is_empty_v<B>) {
        if (!pass_by_value_subobject<B>(offset + static_cast<std::size_t>(std::meta::offset_of(b).bytes))) return false;
      }
    }
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^U, ctx))) {
      if constexpr (std::meta::is_bit_field(m)) {
        const auto off_bits = offset * 8 + static_cast<std::size_t>(std::meta::offset_of(m).bytes) * 8 +
                              static_cast<std::size_t>(std::meta::offset_of(m).bits);
        const auto width = static_cast<std::size_t>(std::meta::bit_size_of(m));
        if (width > 64) return false;
        if (width > 0 && (off_bits / 64) != ((off_bits + width - 1) / 64)) return false;
      } else {
        using M = [:std::meta::type_of(m):];
        if constexpr (!std::is_empty_v<M>) {
          if (!pass_by_value_subobject<M>(offset + reflect::byte_offset(m))) return false;
        }
      }
    }
    return true;
  } else {
    return false;
  }
}
template <class T>
consteval bool pass_by_value() noexcept {
  using U = std::remove_cv_t<T>;
  return layout::CallTrivial<U> && pass_by_value_subobject<U>(0);
}
template <class T>
struct CallTransport {
  using type = std::conditional_t<pass_by_value<T>(), std::remove_cv_t<T>, T&&>;
};
template <class T>
struct CallTransport<T&> {
  using type = T&;
};
template <class T>
struct CallTransport<T&&> {
  using type = T&&;
};
template <auto Fn>
consteval bool valid_target() {
  using F = std::remove_cv_t<decltype(Fn)>;
  return std::is_member_function_pointer_v<F> || std::is_pointer_v<F> ||
         (std::is_empty_v<F> && std::is_trivially_default_constructible_v<F> && std::is_trivially_destructible_v<F>);
}
template <class>
struct MemberOwner;
template <class M, class C>
struct MemberOwner<M C::*> {
  using type = C;
};
template <auto Fn, class T, class... A>
consteval auto object_type() {
  if constexpr (std::is_member_function_pointer_v<decltype(Fn)>) {
    using C = typename MemberOwner<decltype(Fn)>::type;
    constexpr bool constant = std::is_const_v<T> || std::is_invocable_v<decltype(Fn), const C&, A...>;
    return std::type_identity<std::conditional_t<constant, const C, C>>{};
  } else
    return std::type_identity<T>{};
}
template <auto Fn, class T, class... A>
using ObjectOf = typename decltype(object_type<Fn, T, A...>())::type;
template <auto Fn, class... A>
constexpr decltype(auto) invoke_free(A&&... args) noexcept(std::is_nothrow_invocable_v<decltype(Fn), A...>) {
  if constexpr (std::is_pointer_v<decltype(Fn)>)
    return Fn(static_cast<A&&>(args)...);
  else
    return std::remove_cv_t<decltype(Fn)>{}(static_cast<A&&>(args)...);
}
template <auto Fn, class Obj, class... A>
constexpr decltype(auto) invoke_object(Obj& object,
                                       A&&... args) noexcept(std::is_nothrow_invocable_v<decltype(Fn), Obj&, A...>) {
  if constexpr (std::is_member_function_pointer_v<decltype(Fn)>)
    return (object.*Fn)(static_cast<A&&>(args)...);
  else
    return invoke_free<Fn>(object, static_cast<A&&>(args)...);
}
template <class R, class Actual, bool Noexcept, bool Safe>
consteval void validate_result() {
  static_assert(!std::is_void_v<R> || std::is_void_v<Actual>,
                "void callback must not silently discard a non-void return value");
  static_assert(std::is_convertible_v<Actual, R>, "target result is not convertible to callback result");
  static_assert(!std::reference_converts_from_temporary_v<R, Actual>, "result conversion would return a dangling reference");
  static_assert(!Noexcept || (Safe && std::is_nothrow_convertible_v<Actual, R>),
                "noexcept callback requires a noexcept target and conversion");
}
template <class>
struct Signature;
template <bool N, class R, class... A>
struct Signature<R (*)(A...) noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) & noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) const noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) const & noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) && noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <bool N, class R, class C, class... A>
struct Signature<R (C::*)(A...) const && noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <class P>
struct BoundSignature : Signature<P> {};
template <bool N, class R, class First, class... A>
struct BoundSignature<R (*)(First, A...) noexcept(N)> {
  using type = R(A...) noexcept(N);
};
template <auto Fn>
consteval auto signature_type() {
  if constexpr (std::is_pointer_v<decltype(Fn)> || std::is_member_function_pointer_v<decltype(Fn)>)
    return std::type_identity<typename Signature<decltype(Fn)>::type>{};
  else
    return std::type_identity<typename Signature<decltype(+Fn)>::type>{};
}
template <class>
struct NoexceptSignature;
template <bool N, class R, class... A>
struct NoexceptSignature<R(A...) noexcept(N)> {
  using type = R(A...) noexcept;
};
}  // namespace detail

/// Value parameters outside the register ABI subset consume an explicit rvalue; lvalues require T{source}.
template <class T>
using CallArg = typename detail::CallTransport<T>::type;
template <class Signature>
class Delegate;

template <bool Noexcept, class R, class... Args>
class Delegate<R(Args...) noexcept(Noexcept)> {
 public:
  using Stub = R (*)(void*, CallArg<Args>...) noexcept(Noexcept);
  constexpr Delegate() noexcept = default;
  template <bool OtherNoexcept>
    requires(!Noexcept && OtherNoexcept)
  constexpr Delegate(Delegate<R(Args...) noexcept(OtherNoexcept)> other) noexcept
      : context_(other.context()), stub_(other.stub()) {}
  [[nodiscard]] static constexpr Delegate from_raw(Stub stub, void* context) noexcept {
    Delegate result;
    result.context_ = detail::callback_present(stub) ? context : nullptr;
    result.stub_ = stub;
    return result;
  }
  template <auto Fn, class T>
    requires(!std::is_member_object_pointer_v<decltype(Fn)>)
  [[nodiscard]] static constexpr Delegate bind(T& object) noexcept {
    static_assert(detail::valid_target<Fn>(),
                  "stateful closures are not owned; bind a named object lvalue with an explicit member target");
    using Obj = detail::ObjectOf<Fn, T, CallArg<Args>...>;
    constexpr bool receiver = std::is_convertible_v<T*, Obj*> && !detail::PointerReceiver<T>;
    static_assert(receiver, "pass a stable object lvalue (*ptr), not a pointer or smart pointer");
    constexpr bool callable = std::is_invocable_v<decltype(Fn), Obj&, CallArg<Args>...>;
    static_assert(callable, "target is not invocable on this receiver; check const and ref qualifiers");
    if constexpr (receiver && callable) {
      using Actual = std::invoke_result_t<decltype(Fn), Obj&, CallArg<Args>...>;
      detail::validate_result<R, Actual, Noexcept, std::is_nothrow_invocable_v<decltype(Fn), Obj&, CallArg<Args>...>>();
      // Erase the adjusted subobject, so every derived receiver shares the same hot stub.
      Obj* typed = __builtin_addressof(object);
      return from_raw(&object_stub<Fn, Obj>, const_cast<void*>(static_cast<const void*>(typed)));
    } else
      return {};
  }
  template <auto Fn, class... T>
    requires(std::is_member_object_pointer_v<decltype(Fn)>)
  static Delegate bind(T&&...) = delete ("member data pointers are not callbacks; bind a member function");
  template <auto Fn, class T>
  static Delegate bind(T*) = delete ("pass a stable object lvalue (*ptr), not a pointer or smart pointer");
  template <auto Fn, class T>
    requires(!std::is_lvalue_reference_v<T>)
  static Delegate bind(T&&) = delete ("Delegate does not own the target; bind an lvalue that outlives the Delegate");
  template <auto Fn>
    requires(!std::is_member_object_pointer_v<decltype(Fn)>)
  [[nodiscard]] static constexpr Delegate bind() noexcept {
    constexpr bool target = detail::valid_target<Fn>();
    static_assert(target, "stateful closures are not owned; bind a named object lvalue with an explicit member target");
    if constexpr (std::is_pointer_v<decltype(Fn)>) static_assert(detail::callback_present(Fn), "null function target");
    constexpr bool callable = std::is_invocable_v<decltype(Fn), CallArg<Args>...>;
    static_assert(callable, "target is not invocable with callback arguments");
    if constexpr (target && callable) {
      using Actual = std::invoke_result_t<decltype(Fn), CallArg<Args>...>;
      detail::validate_result<R, Actual, Noexcept, std::is_nothrow_invocable_v<decltype(Fn), CallArg<Args>...>>();
      return from_raw(&function_stub<Fn>, nullptr);
    } else
      return {};
  }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return detail::callback_present(stub_); }
  [[nodiscard]] constexpr Stub stub() const noexcept { return stub_; }
  [[nodiscard]] constexpr void* context() const noexcept { return context_; }
  constexpr void reset() noexcept {
    context_ = nullptr;
    stub_ = nullptr;
  }
  // This compares context/stub slots; --icf=all can fold distinct targets sharing one context into equality.
  friend constexpr bool operator==(Delegate, Delegate) noexcept = default;
  /// The target must be nonempty and alive. Large/nontrivial values require std::move(value) or T{source}.
  [[gnu::always_inline]] constexpr R operator()(CallArg<Args>... args) const noexcept(Noexcept) {
    return stub_(context_, static_cast<CallArg<Args>&&>(args)...);
  }

 private:
  template <auto Fn, class Obj>
  static constexpr R object_stub(void* context, CallArg<Args>... args) noexcept(Noexcept) {
    return detail::invoke_object<Fn>(*static_cast<Obj*>(context), static_cast<CallArg<Args>&&>(args)...);
  }
  template <auto Fn>
  static constexpr R function_stub(void*, CallArg<Args>... args) noexcept(Noexcept) {
    return detail::invoke_free<Fn>(static_cast<CallArg<Args>&&>(args)...);
  }
  void* context_{nullptr};
  Stub stub_{nullptr};
};

template <class Signature>
using Callback = Delegate<typename detail::NoexceptSignature<Signature>::type>;
// Free binding preserves target noexcept; Clang 21-p2996 explicit-object NTTPs need an object-first lambda.
template <auto Fn>
  requires(!std::is_member_object_pointer_v<decltype(Fn)>)
[[nodiscard]] constexpr auto bind() noexcept {
  using S = typename decltype(detail::signature_type<Fn>())::type;
  return Delegate<S>::template bind<Fn>();
}
template <auto Fn, class T>
  requires(!std::is_member_object_pointer_v<decltype(Fn)>)
[[nodiscard]] constexpr auto bind(T& object) noexcept {
  using S = typename detail::BoundSignature<decltype(Fn)>::type;
  return Delegate<S>::template bind<Fn>(object);
}
template <auto Fn, class... T>
  requires(std::is_member_object_pointer_v<decltype(Fn)>)
auto bind(T&&...) = delete ("member data pointers are not callbacks; bind a member function");
template <auto Fn, class T>
auto bind(T*) = delete ("pass a stable object lvalue (*ptr), not a pointer or smart pointer");
template <auto Fn, class T>
  requires(!std::is_lvalue_reference_v<T>)
auto bind(T&&) = delete ("Delegate does not own the target; bind an lvalue that outlives the Delegate");
template <class F>
  requires(std::is_empty_v<F> && std::is_default_constructible_v<F> && requires { +F{}; })
[[nodiscard]] constexpr auto bind(F) noexcept {
  return bind<+F{}>();
}
template <class F>
  requires(!std::is_empty_v<F> && !std::is_pointer_v<F>)
auto bind(F) = delete ("stateful closures are not owned; bind a named object lvalue with an explicit member target");
template <bool N, class R, class... A>
auto bind(R (*)(A...) noexcept(N)) = delete ("runtime function pointers are not compile-time targets; use bind<&fn>()");
}  // namespace uarch::flow
