#pragma once
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <utility>
namespace uarch::views {
/// Forward chunks retain references and include the final partial chunk.
template <std::ranges::forward_range V>
  requires std::ranges::view<V>
class ChunkView : public std::ranges::view_interface<ChunkView<V>> {
  V base_{};
  std::ranges::range_difference_t<V> count_{1};

 public:
  ChunkView()
    requires std::default_initializable<V>
  = default;
  constexpr ChunkView(V v, std::ranges::range_difference_t<V> n) : base_(std::move(v)), count_(n) {
    if (n <= 0) std::abort();
  }
  class iterator {
    std::ranges::iterator_t<V> current_{};
    std::ranges::sentinel_t<V> end_{};
    std::ranges::range_difference_t<V> count_{1};

   public:
    using difference_type = std::ranges::range_difference_t<V>;
    using value_type = std::ranges::subrange<std::ranges::iterator_t<V>>;
    using iterator_concept = std::forward_iterator_tag;
    iterator() = default;
    constexpr iterator(std::ranges::iterator_t<V> it, std::ranges::sentinel_t<V> end, difference_type n)
        : current_(it), end_(end), count_(n) {}
    constexpr value_type operator*() const { return {current_, std::ranges::next(current_, count_, end_)}; }
    constexpr iterator& operator++() {
      std::ranges::advance(current_, count_, end_);
      return *this;
    }
    constexpr iterator operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }
    friend constexpr bool operator==(const iterator& a, const iterator& b) { return a.current_ == b.current_; }
    friend constexpr bool operator==(const iterator& a, std::default_sentinel_t) { return a.current_ == a.end_; }
  };
  constexpr auto begin() { return iterator(std::ranges::begin(base_), std::ranges::end(base_), count_); }
  [[nodiscard]] constexpr auto end() const { return std::default_sentinel; }
};
class ChunkClosure : public std::ranges::range_adaptor_closure<ChunkClosure> {
  std::ptrdiff_t count_;

 public:
  constexpr explicit ChunkClosure(std::ptrdiff_t n) : count_(n) {}
  template <std::ranges::viewable_range R>
    requires std::ranges::forward_range<R>
  constexpr auto operator()(R&& r) const {
    using V = std::views::all_t<R>;
    return ChunkView<V>(std::views::all(std::forward<R>(r)), count_);
  }
};
constexpr auto chunk(std::ptrdiff_t n) { return ChunkClosure(n); }
}  // namespace uarch::views
