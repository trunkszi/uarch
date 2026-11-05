#include <limits>
#include <uarch/core/simd/vec.hpp>
using V = uarch::simd::vec<std::int64_t, 4>;
constexpr auto kValues = V::from_array({1, 2, 3, 4});
static_assert(uarch::simd::reduce(kValues) == 10);
static_assert(uarch::simd::reduce_min(kValues) == 1);
static_assert(uarch::simd::reduce_max(kValues) == 4);
static_assert(uarch::simd::find_first_set(kValues >= 3) == 2);
static_assert(uarch::simd::reduce(uarch::simd::select(kValues < 3, kValues, V{0})) == 3);
int main() { return 0; }
template <class T, int N>
consteval bool ShapeChecks() {
  using W = uarch::simd::vec<T, N>;
  std::array<T, static_cast<std::size_t>(N)> input{};
  for (int i = 0; i < N; ++i) input[static_cast<std::size_t>(i)] = (static_cast<T>(i) + T{1});
  auto loaded = uarch::simd::unchecked_load<W>(input.data());
  if (!uarch::simd::all_of(loaded == W::from_array(input)) || uarch::simd::reduce_min(loaded) != T{1}) return false;
  if (uarch::simd::reduce_max(loaded) != static_cast<T>(N) || uarch::simd::reduce_count(loaded < T{3}) != 2) return false;
  if (!uarch::simd::none_of((loaded < T{3}) & !(loaded < T{3}))) return false;
  if (!uarch::simd::all_of((loaded + T{1} - T{1}) == loaded)) return false;
  return uarch::simd::reduce(loaded) == (static_cast<T>(N) * static_cast<T>(N + 1) / T{2});
}
static_assert(ShapeChecks<std::int32_t, 8>() && ShapeChecks<std::uint64_t, 2>() && ShapeChecks<double, 4>());
constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
static_assert(uarch::simd::all_of((V{kMax} + V{1}) == V{kMin}));
static_assert(uarch::simd::all_of((V{kMin} - V{1}) == V{kMax}));
static_assert(uarch::simd::all_of((V{kMax} * V{2}) == V{-2}));
struct Tier32 {
  static constexpr std::size_t vector_bytes = 32;
};
struct Tier64 {
  static constexpr std::size_t vector_bytes = 64;
};
static_assert(uarch::simd::native_vec<std::int64_t, Tier32>::size() == 4 &&
              uarch::simd::native_vec<double, Tier64>::size() == 8);
