#include <array>
#include <bit>
#include <limits>
#include <uarch/arch/entry.hpp>
#include <uarch/core/simd/vec.hpp>

using namespace uarch::simd;
template <class T, int N>
bool CheckLanes() {
  std::array<T, static_cast<std::size_t>(N)> a{}, b{};
  for (int i = 0; i < N; ++i) {
    a[static_cast<std::size_t>(i)] = (static_cast<T>(i) + T{1});
    b[static_cast<std::size_t>(i)] = (static_cast<T>(i % 3) + T{1});
  }
  using V = vec<T, N>;
  auto x = V::from_array(a), y = unchecked_load<V>(b.data());
  auto sum = (x + y).to_array();
  auto product = (x * y).to_array();
  auto difference = (x - y).to_array();
  for (int i = 0; i < N; ++i) {
    auto index = static_cast<std::size_t>(i);
    if (sum[index] != static_cast<T>(a[index] + b[index]) || product[index] != static_cast<T>(a[index] * b[index]))
      return false;
    if constexpr (std::unsigned_integral<T>) {
      if (difference[index] != static_cast<T>(a[index] - b[index])) return false;
    } else if (difference[index] != a[index] - b[index])
      return false;
    if ((x < y)[i] != (a[index] < b[index]) || (x <= y)[i] != (a[index] <= b[index]) || (x > y)[i] != (a[index] > b[index]) ||
        (x >= y)[i] != (a[index] >= b[index]) || (x == y)[i] != (a[index] == b[index]) || (x != y)[i] != (a[index] != b[index]))
      return false;
    if ((x < T{3})[i] != (a[index] < T{3}) || (T{3} <= x)[i] != (T{3} <= a[index])) return false;
  }
  auto less = x < y;
  if (!all_of(less | !less) || any_of(less & ~less) || !none_of(less ^ less)) return false;
  if (find_first_set(mask<T, N>{false}) || reduce_count(mask<T, N>{true}) != static_cast<unsigned>(N)) return false;
  auto selected = select(x < T{3}, x, y);
  for (int i = 0; i < N; ++i)
    if (selected[i] !=
        (a[static_cast<std::size_t>(i)] < T{3} ? a[static_cast<std::size_t>(i)] : b[static_cast<std::size_t>(i)]))
      return false;
  if constexpr (std::integral<T>) {
    if (!all_of(((x & y) | (x ^ y)) == (x | y)) || !all_of((~~x) == x)) return false;
  }
  if constexpr (std::integral<T>) {
    using U = std::make_unsigned_t<T>;
    std::uint64_t expected = 0;
    for (const auto lane : a) expected += static_cast<U>(lane);
    if (std::bit_cast<U>(reduce(x)) != static_cast<U>(expected)) return false;
  } else if (reduce(x) != static_cast<T>(N) * static_cast<T>(N + 1) / T{2})
    return false;
  return reduce_min(x) == T{1} && reduce_max(x) == static_cast<T>(N);
}

[[gnu::always_inline]] constexpr std::int64_t MaskedSum(const std::int64_t* keys, const std::int64_t* values,
                                                        std::int64_t limit) noexcept {
  using V = vec<std::int64_t, 4>;
  return reduce(select(unchecked_load<V>(keys) < limit, unchecked_load<V>(values), V{0}));
}
#if defined(__x86_64__)
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::int64_t V3MaskedSum(const std::int64_t* keys,
                                                                                     const std::int64_t* values,
                                                                                     std::int64_t limit) noexcept {
  return MaskedSum(keys, values, limit);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] int V3FirstGe(const std::int64_t* keys,
                                                                          std::int64_t limit) noexcept {
  auto result = find_first_set(unchecked_load<vec<std::int64_t, 4>>(keys) >= limit);
  return result ? static_cast<int>(*result) : -1;
}
extern "C" [[gnu::target("arch=x86-64-v4"), gnu::noinline]] std::int64_t V4MaskedSum(const std::int64_t* keys,
                                                                                     const std::int64_t* values,
                                                                                     std::int64_t limit) noexcept {
  using V = vec<std::int64_t, 8>;
  return reduce(select(unchecked_load<V>(keys) < limit, unchecked_load<V>(values), V{0}));
}
#endif
static_assert(uarch::arch::detail::DirectEntryValue<void>);
static_assert(uarch::arch::detail::DirectEntryValue<vec<int, 4>>);
static_assert(!uarch::arch::detail::DirectEntryValue<vec<int, 8>>);
static_assert(std::same_as<uarch::arch::detail::EntryParam<vec<int, 8>>, const vec<int, 8>&>);
#if defined(__x86_64__)
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline vec<int, 8> V3Increment(uarch::arch::X86V3,
                                                                                     vec<int, 8> value) noexcept {
  return value + vec<int, 8>{1};
}
#endif
int main() {
  if (!CheckLanes<std::int8_t, 16>() || !CheckLanes<std::uint8_t, 16>() || !CheckLanes<std::int16_t, 8>() ||
      !CheckLanes<std::uint16_t, 8>() || !CheckLanes<std::int32_t, 8>() || !CheckLanes<std::uint32_t, 4>() ||
      !CheckLanes<std::int64_t, 4>() || !CheckLanes<std::uint64_t, 2>() || !CheckLanes<float, 4>() || !CheckLanes<double, 4>())
    return 1;
  const auto cancellation = vec<float, 4>::from_array({1.0e20F, 1.0F, -1.0e20F, 1.0F});
  if (reduce(cancellation) != 0.0F || reduce(cancellation, [](float a, float b) { return a + b; }) != 0.0F) return 9;
  const auto wrapped = vec<std::int32_t, 4>::from_array({INT32_MAX, 1, INT32_MAX, 1});
  if (reduce(wrapped) != 0) return 10;
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  std::array<std::int64_t, 4> keys{kMin, -1, 0, kMax}, values{kMax, 1, 9, 37};
  if (MaskedSum(keys.data(), values.data(), 0) != kMin || MaskedSum(keys.data(), values.data(), kMin) != 0 ||
      MaskedSum(keys.data(), values.data(), kMax) != kMin + 9)
    return 2;
  std::array<std::uint64_t, 3> offset{99, 3, 7};
  if (reduce(unchecked_load<vec<std::uint64_t, 2>>(std::span<const std::uint64_t, 2>{offset.data() + 1, 2})) != 10) return 3;
  if (!all_of((vec<std::int64_t, 4>{kMax} + vec<std::int64_t, 4>{1}) == vec<std::int64_t, 4>{kMin})) return 4;
#if defined(__x86_64__)
  if (__builtin_cpu_supports("x86-64-v3")) {
    auto token = uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag);
    auto result = uarch::arch::run<V3Increment>(token, vec<int, 8>::from_array({1, 2, 3, 4, 5, 6, 7, 8})).to_array();
    for (std::size_t i = 0; i < result.size(); ++i)
      if (result[i] != static_cast<int>(i) + 2) return 8;
    if (V3MaskedSum(keys.data(), values.data(), 0) != kMin || V3FirstGe(keys.data(), kMin) != 0 ||
        V3FirstGe(keys.data(), 0) != 2 || V3FirstGe(keys.data(), kMax) != 3)
      return 5;
    std::array<std::int64_t, 4> unordered{7, 2, 9, 1};
    if (V3FirstGe(unordered.data(), 8) != 2 || V3FirstGe(unordered.data(), 10) != -1) return 6;
  }
  if (__builtin_cpu_supports("x86-64-v4")) {
    std::array<std::int64_t, 8> wide_keys{0, 1, 2, 3, 4, 5, 6, 7}, wide_values{1, 2, 3, 4, 5, 6, 7, 8};
    if (V4MaskedSum(wide_keys.data(), wide_values.data(), 4) != 10) return 7;
  }
#endif
  return 0;
}
