#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <uarch/core/flow/delegate.hpp>
#include <uarch/core/mem/pool_index.hpp>
#include <uarch/core/text.hpp>
#include <uarch/primitives.hpp>
#include <variant>
using uarch::flow::Callback;
using uarch::flow::Delegate;
using D = Callback<int(int)>;
static_assert(sizeof(D) == 16 && alignof(D) == alignof(void*));
static_assert(std::is_standard_layout_v<D> && std::is_trivially_copyable_v<D> && std::is_trivially_destructible_v<D>);
static_assert(std::meta::bases_of(^^D, std::meta::access_context::current()).empty());
static_assert(uarch::layout::NoPadding<D> && uarch::layout::GprPair<D> && uarch::layout::RegisterPassable<D>);
static_assert(uarch::layout::aapcs64_classify<D>() ==
              uarch::layout::AbiShape{uarch::layout::AbiClass::Integer, uarch::layout::AbiClass::Integer, 2});
struct Embedded {
  std::uint32_t tag, flags;
  Callback<void(int)> cb;
};
static_assert(sizeof(Embedded) == 24 && uarch::layout::NoPadding<Embedded>);
static_assert(std::is_same_v<Callback<int(int)>, Callback<int(int) noexcept>>);
static_assert(std::is_convertible_v<D, Delegate<int(int)>> && !std::is_convertible_v<Delegate<int(int)>, D>);
static_assert(std::is_same_v<uarch::flow::CallArg<int>, int>);
static_assert(std::is_same_v<uarch::flow::CallArg<bool>, bool>);
static_assert(std::is_same_v<uarch::flow::CallArg<std::optional<int>>, std::optional<int>>);
static_assert(std::is_same_v<uarch::flow::CallArg<std::expected<int, int>>, std::expected<int, int>>);
static_assert(std::is_same_v<uarch::flow::CallArg<std::variant<int, long>>, std::variant<int, long>>);
struct [[gnu::packed]] Packed {
  char prefix;
  std::uint64_t value;
};
// Native bit-fields are controls for the ABI classifier, not payload specifications.
struct BitFields {
  unsigned low : 31;
  unsigned high : 1;
};
struct [[gnu::packed]] CrossingBits {
  std::uint64_t prefix : 1;
  std::uint64_t wide : 64;
};
struct ScalarBase {
  std::uint64_t base;
};
struct SmallDerived : ScalarBase {
  std::uint32_t value;
};
struct Bytes {
  char data[16];
};
union Number {
  int integer;
  double real;
};
static_assert(std::is_same_v<uarch::flow::CallArg<BitFields>, BitFields>);
static_assert(std::is_same_v<uarch::flow::CallArg<CrossingBits>, CrossingBits&&>);
static_assert(std::is_same_v<uarch::flow::CallArg<SmallDerived>, SmallDerived>);
static_assert(!uarch::layout::CallTrivial<char[16]>);
static_assert(!uarch::flow::detail::pass_by_value<char[16]>());
static_assert(std::is_same_v<uarch::flow::CallArg<std::array<char, 16>>, std::array<char, 16>>);
static_assert(std::is_same_v<uarch::flow::CallArg<Bytes>, Bytes>);
static_assert(std::is_same_v<uarch::flow::CallArg<uarch::text::FixedAscii<16>>, uarch::text::FixedAscii<16>>);
static_assert(std::is_same_v<uarch::flow::CallArg<Number>, Number>);
static_assert(uarch::layout::sysv_classify<Number>().lo == uarch::layout::AbiClass::Memory);
static_assert(!uarch::layout::RegisterPassable<bool>);
struct BigStruct {
  std::uint64_t words[8];
};
static_assert(std::is_same_v<uarch::flow::CallArg<Packed>, Packed&&>);
static_assert(std::is_same_v<uarch::flow::CallArg<long double>, long double&&>);
static_assert(std::is_same_v<uarch::flow::CallArg<std::expected<BigStruct, int>>, std::expected<BigStruct, int>&&>);
static_assert(std::is_same_v<uarch::flow::CallArg<std::span<const int>>, std::span<const int>>);
static_assert(std::is_same_v<uarch::flow::CallArg<const int&>, const int&>);
using Index = uarch::mem::PoolIndex<struct Tag>;
static_assert(std::is_same_v<uarch::flow::CallArg<Index>, Index>);
struct First {
  std::uint64_t first{3};
};
struct Second {
  std::uint64_t second{7};
  constexpr int read(int x) const& noexcept { return static_cast<int>(second) + x; }
  constexpr int write(int x) & noexcept {
    second = static_cast<std::uint64_t>(x);
    return x;
  }
};
static_assert(std::is_same_v<uarch::flow::CallArg<decltype(&Second::read)>, decltype(&Second::read)>);
struct Derived : First, Second {};
struct Other : First, Second {};
constexpr int free_target(int x) noexcept { return x + 2; }
constexpr int free_noexcept_fn(int x) noexcept { return x; }
constexpr int free_may_throw_fn(int x) { return x; }
static_assert(std::same_as<decltype(uarch::bind<&free_noexcept_fn>()), uarch::Callback<int(int)>>);
static_assert(std::same_as<decltype(uarch::bind<&free_may_throw_fn>()), uarch::Delegate<int(int)>>);
static_assert(uarch::bind<&free_noexcept_fn>()(4) == 4 && uarch::bind<&free_may_throw_fn>()(5) == 5);
struct Big {
  int* moves;
  int* destroyed;
  std::uint64_t words[4]{};
  constexpr Big(int& m, int& d) : moves(&m), destroyed(&d) {}
  constexpr Big(Big&& other) noexcept : moves(other.moves), destroyed(other.destroyed) { ++*moves; }
  constexpr ~Big() noexcept { ++*destroyed; }
};
struct Receiver {
  constexpr int consume(Big value) noexcept { return *value.moves; }
  constexpr int borrow(Big&& value) noexcept { return *value.moves; }
  constexpr int constant(const Big& value) noexcept { return *value.moves; }
};
static_assert(std::is_same_v<uarch::flow::CallArg<Big>, Big&&>);
static_assert(!std::is_invocable_v<Callback<int(Big)>, Big&>);
static_assert(!std::is_invocable_v<Callback<void(Big)>, Big&> && std::is_invocable_v<Callback<void(Big)>, Big&&>);
consteval bool validate() {
  Derived derived;
  Other other;
  const Derived constant;
  Second base;
  const auto d = D::bind<&Second::read>(derived);
  if (!d || !D::from_raw(d.stub(), d.context())) return false;
  const auto b = D::bind<&Second::read>(base);
  const auto c = D::bind<&Second::read>(constant);
  const auto o = D::bind<&Second::read>(other);
  if (d(2) != 9 || c(3) != 10 || d.stub() != b.stub() || o.stub() != d.stub()) return false;
  if (d.context() != static_cast<const void*>(static_cast<const Second*>(&derived))) return false;
  Delegate<int(int)> relaxed = d;
  if (relaxed(4) != 11 || D::bind<&free_target>()(2) != 4) return false;
  auto mutable_call = uarch::flow::bind<&Second::write>(base);
  if (mutable_call(5) != 5 || base.second != 5) return false;
  if (uarch::flow::bind([](int x) noexcept { return x + 3; })(2) != 5) return false;
  if (D::bind<[](int x) noexcept { return x + 1; }>()(2) != 3) return false;
  auto object_first = D::bind<[](const Second& self, int x) noexcept { return self.read(x); }>(base);
  if (object_first(2) != 7) return false;
  auto empty = D::from_raw(nullptr, &derived);
  if (empty || empty.context() || empty != D{}) return false;
  auto reset = d;
  reset.reset();
  if (reset != D{}) return false;
  int moves = 0, dead = 0;
  Big big(moves, dead);
  Receiver receiver;
  auto consuming = Callback<int(Big)>::bind<&Receiver::consume>(receiver);
  if (consuming(static_cast<Big&&>(big)) != 1 || moves != 1 || dead != 1) return false;
  moves = 0;
  dead = 0;
  if (Callback<int(Big)>::bind<&Receiver::borrow>(receiver)(static_cast<Big&&>(big)) != 0 || dead != 0) return false;
  return Callback<int(Big)>::bind<&Receiver::constant>(receiver)(static_cast<Big&&>(big)) == 0 && moves == 0;
}
static_assert(validate());
struct ExplicitReceiver {
  int value{9};
  constexpr int invoke(this const ExplicitReceiver& self, int x) noexcept { return self.value + x; }
};
consteval bool explicit_object() {
  ExplicitReceiver receiver;
  auto portable = D::bind<[](const ExplicitReceiver& self, int x) noexcept { return self.invoke(x); }>(receiver);
  if (portable(3) != 12) return false;
// Clang 21-p2996 rejects explicit-object function addresses as NTTPs; GCC accepts them.
#if defined(__GNUC__) && !defined(__clang__)
  static_assert(std::same_as<decltype(uarch::bind<&ExplicitReceiver::invoke>(receiver)), D>);
  auto direct = uarch::flow::bind<&ExplicitReceiver::invoke>(receiver);
  if (direct(3) != 12) return false;
#endif
  return true;
}
static_assert(explicit_object());
struct WrapperReceiver {
  constexpr int boolean(bool value) const noexcept { return value ? 1 : -1; }
  constexpr int optional(std::optional<int> value) const noexcept { return value.value_or(-1); }
  constexpr int expected(std::expected<int, int> value) const noexcept { return value ? *value : -1; }
  constexpr int variant(std::variant<int, long> value) const noexcept {
    if (const auto* number = std::get_if<int>(&value)) return *number;
    return static_cast<int>(*std::get_if<long>(&value));
  }
};
consteval bool wrapper_calls() {
  WrapperReceiver receiver;
  return Callback<int(bool)>::bind<&WrapperReceiver::boolean>(receiver)(true) == 1 &&
         Callback<int(std::optional<int>)>::bind<&WrapperReceiver::optional>(receiver)(std::optional{7}) == 7 &&
         Callback<int(std::expected<int, int>)>::bind<&WrapperReceiver::expected>(receiver)(std::expected<int, int>{9}) == 9 &&
         Callback<int(std::variant<int, long>)>::bind<&WrapperReceiver::variant>(receiver)(std::variant<int, long>{11L}) == 11;
}
static_assert(wrapper_calls());

int main() {}
