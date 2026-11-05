/** @file @brief Owning bitmap snapshot view and pipeable set-bit iteration. */
#pragma once

#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <uarch/core/bits/bitmap64.hpp>

namespace uarch::views {
/** @brief Ascending set-bit indexes; iterators own their remaining-bit snapshot. */
class BitOnesView : public std::ranges::view_interface<BitOnesView> {
  std::uint64_t bits_{};

 public:
  /** @brief Multi-pass iterator whose lifetime is independent of the originating view. */
  class Iterator {
    std::uint64_t remaining_{};

   public:
    using value_type = std::uint32_t;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    constexpr Iterator() noexcept = default;
    explicit constexpr Iterator(std::uint64_t bits) noexcept : remaining_(bits) {}
    [[nodiscard]] constexpr value_type operator*() const noexcept {
      if (remaining_ == 0) std::abort();
      return static_cast<value_type>(std::countr_zero(remaining_));
    }
    constexpr Iterator& operator++() noexcept {
      remaining_ &= remaining_ - 1;
      return *this;
    }
    constexpr Iterator operator++(int) noexcept {
      auto old = *this;
      ++*this;
      return old;
    }
    constexpr bool operator==(const Iterator&) const noexcept = default;
  };
  constexpr BitOnesView() noexcept = default;
  explicit constexpr BitOnesView(std::uint64_t bits) noexcept : bits_(bits) {}
  [[nodiscard]] constexpr Iterator begin() const noexcept { return Iterator{bits_}; }
  [[nodiscard]] constexpr Iterator end() const noexcept {
    static_cast<void>(bits_);
    return Iterator{};
  }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return static_cast<std::size_t>(std::popcount(bits_)); }
};
/** @brief Adapt bitmap values, bitsets, or raw u64 values to an owning snapshot. */
struct Ones : std::ranges::range_adaptor_closure<Ones> {
  [[nodiscard]] static constexpr BitOnesView operator()(std::uint64_t raw) noexcept { return BitOnesView{raw}; }
  [[nodiscard]] static constexpr BitOnesView operator()(bits::Bitmap64 bitmap) noexcept { return BitOnesView{bitmap.raw()}; }
  [[nodiscard]] static constexpr BitOnesView operator()(std::bitset<64> bitmap) noexcept {
    return BitOnesView{bitmap.to_ullong()};
  }
};
inline constexpr Ones ones{};

/** @brief Only the ones adaptor owns non-range pipe syntax; unrelated range factories stay untouched. */
[[nodiscard]] constexpr BitOnesView operator|(std::uint64_t input, Ones) noexcept { return Ones{}(input); }
[[nodiscard]] constexpr BitOnesView operator|(bits::Bitmap64 input, Ones) noexcept { return Ones{}(input); }
[[nodiscard]] constexpr BitOnesView operator|(std::bitset<64> input, Ones) noexcept { return Ones{}(input); }
}  // namespace uarch::views

template <>
inline constexpr bool std::ranges::enable_borrowed_range<uarch::views::BitOnesView> = true;
