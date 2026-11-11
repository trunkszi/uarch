#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
namespace uarch::flow {
namespace detail {
template <std::size_t N>
consteval auto indices() {
  std::array<std::size_t, N> out{};
  for (std::size_t i = 0; i < N; ++i) out[i] = i;
  return out;
}
template <std::size_t N>
inline constexpr auto lane_indices = indices<N>();
}  // namespace detail
/// Step and combine must admit an identity and associative, commutative reassociation.
/// Each input is visited once; init is combined only at the root.
template <std::size_t N, class T, class A, class Step, class Combine>
constexpr A reduce_lanes(std::span<const T> xs, A init, A identity, Step step, Combine combine) {
  static_assert(N == 2uz || N == 4uz || N == 8uz, std::string_view("reduce_lanes lane count must be 2, 4, or 8"));
  std::array<A, N> accum = [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::array<A, N>{((void)I, identity)...};
  }(std::make_index_sequence<N>{});
  std::size_t base = 0;
  for (; xs.size() - base >= N; base += N) {
    template for (constexpr auto lane : detail::lane_indices<N>) accum[lane] = step(accum[lane], xs[base + lane]);
  }
  template for (constexpr auto lane : detail::lane_indices<N>) {
    if (lane < xs.size() - base) accum[lane] = step(accum[lane], xs[base + lane]);
  }
  for (std::size_t stride = 1; stride < N; stride *= 2)
    for (std::size_t i = 0; i < N; i += 2 * stride) accum[i] = combine(accum[i], accum[i + stride]);
  return combine(init, accum[0]);
}
template <class T, class A, class Step, class Combine>
constexpr A reduce_four_way(std::span<const T> xs, A init, A identity, Step step, Combine combine) {
  return reduce_lanes<4>(xs, init, identity, step, combine);
}
}  // namespace uarch::flow
