#include <array>
#include <bit>
#include <cstdlib>
#include <uarch/core/arithmetic/saturating.hpp>

using namespace uarch::arithmetic;
template <ArithmeticInteger T>
void check(T a, T b) {
  using Wide = std::conditional_t<std::is_signed_v<T>, __int128_t, __uint128_t>;
  const auto widen = [](T value) -> Wide {
    using U = std::make_unsigned_t<T>;
    const auto bits = std::bit_cast<U>(value);
    if constexpr (std::is_signed_v<T>) {
      if (value < 0) return -static_cast<Wide>(static_cast<U>(U{0} - bits));
    }
    return static_cast<Wide>(bits);
  };
  const auto low = widen(std::numeric_limits<T>::min());
  const auto high = widen(std::numeric_limits<T>::max());
  const auto x = widen(a);
  const auto y = widen(b);
  for (unsigned op = 0; op < 3; ++op) {
    const bool underflow = !std::is_signed_v<T> && op == 1 && a < b;
    const Wide exact = op == 0 ? x + y : (op == 1 ? x - y : x * y);
    const bool valid = !underflow && exact >= low && exact <= high;
    const T expected = underflow || exact < low ? std::numeric_limits<T>::min()
                                                : (exact > high ? std::numeric_limits<T>::max() : static_cast<T>(exact));
    const T saturated = op == 0 ? saturating_add(a, b) : (op == 1 ? saturating_sub(a, b) : saturating_mul(a, b));
    const auto checked = op == 0 ? checked_add(a, b) : (op == 1 ? checked_sub(a, b) : checked_mul(a, b));
    if (saturated != expected || checked.has_value() != valid || (checked && *checked != expected)) std::abort();
  }
  if (b != 0) {
    const auto q = x / y;
    const auto expected = static_cast<T>(q > high ? high : q);
    if (saturating_div(a, b) != expected) std::abort();
  }
}
int main() {
  for (int a = -128; a <= 127; ++a)
    for (int b = -128; b <= 127; ++b) check(static_cast<std::int8_t>(a), static_cast<std::int8_t>(b));
  for (unsigned a = 0; a <= 255; ++a)
    for (unsigned b = 0; b <= 255; ++b) check(static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b));
  constexpr std::array<std::int64_t, 7> signed_edges{INT64_MIN, INT64_MIN + 1, -1, 0, 1, INT64_MAX - 1, INT64_MAX};
  for (auto a : signed_edges)
    for (auto b : signed_edges) check(a, b);
  constexpr std::array<std::uint64_t, 6> unsigned_edges{0, 1, 2, UINT64_MAX, UINT64_MAX - 1, std::uint64_t{1} << 63};
  for (auto a : unsigned_edges)
    for (auto b : unsigned_edges) check(a, b);
  if (saturating_cast<std::uint8_t>(-1) != 0 || saturating_cast<std::int8_t>(UINT64_MAX) != INT8_MAX ||
      saturating_cast<std::int64_t>(UINT64_MAX) != INT64_MAX || saturating_cast<std::uint64_t>(INT64_MIN) != 0)
    return 1;
}
