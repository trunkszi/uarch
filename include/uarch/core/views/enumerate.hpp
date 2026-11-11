#pragma once
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
namespace uarch::views {
template <std::ranges::forward_range V>
  requires std::ranges::view<V>
class EnumerateView : public std::ranges::view_interface<EnumerateView<V>> {
  V base_{};

 public:
  EnumerateView()
    requires std::default_initializable<V>
  = default;
  constexpr explicit EnumerateView(V v) : base_(std::move(v)) {}
  class iterator {
    std::ranges::iterator_t<V> current_{};
    std::ranges::range_difference_t<V> index_{0};

   public:
    using difference_type = std::ranges::range_difference_t<V>;
    using value_type = std::tuple<difference_type, std::ranges::range_value_t<V>>;
    using iterator_concept = std::forward_iterator_tag;
    iterator() = default;
    constexpr iterator(std::ranges::iterator_t<V> it, difference_type i) : current_(it), index_(i) {}
    constexpr auto operator*() const {
      return std::tuple<difference_type, std::ranges::range_reference_t<V>>(index_, *current_);
    }
    constexpr iterator& operator++() {
      ++current_;
      ++index_;
      return *this;
    }
    constexpr iterator operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }
    friend constexpr bool operator==(const iterator& a, const iterator& b) { return a.current_ == b.current_; }
    [[nodiscard]] constexpr const auto& base() const { return current_; }
  };
  class sentinel {
    std::ranges::sentinel_t<V> end_{};

   public:
    sentinel() = default;
    constexpr explicit sentinel(std::ranges::sentinel_t<V> end) : end_(end) {}
    friend constexpr bool operator==(const iterator& it, const sentinel& s) { return it.base() == s.end_; }
  };
  constexpr auto begin() { return iterator(std::ranges::begin(base_), 0); }
  constexpr auto end() { return sentinel(std::ranges::end(base_)); }
};
struct EnumerateClosure : std::ranges::range_adaptor_closure<EnumerateClosure> {
  template <std::ranges::viewable_range R>
    requires std::ranges::forward_range<R>
  constexpr auto operator()(R&& r) const {
    return EnumerateView<std::views::all_t<R>>(std::views::all(std::forward<R>(r)));
  }
};
inline constexpr EnumerateClosure enumerate{};
}  // namespace uarch::views
