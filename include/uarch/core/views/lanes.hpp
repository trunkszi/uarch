#pragma once
#include <uarch/core/views/chunk.hpp>
namespace uarch::views {
/// Fixed-width reference chunks; the last group may contain fewer than N lanes.
template <std::size_t N>
struct LanesClosure : std::ranges::range_adaptor_closure<LanesClosure<N>> {
  static_assert(N > 0);
  template <std::ranges::viewable_range R>
    requires std::ranges::forward_range<R>
  constexpr auto operator()(R&& r) const {
    return chunk(static_cast<std::ptrdiff_t>(N))(std::forward<R>(r));
  }
};
template <std::size_t N>
inline constexpr LanesClosure<N> lanes{};
}  // namespace uarch::views
