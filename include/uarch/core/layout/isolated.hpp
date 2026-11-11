/** @file @brief 128-byte storage alignment with cv/ref-preserving access. */
#pragma once
#include <compare>
#include <memory>
#include <utility>

namespace uarch::layout {
/// Alignment may introduce tail padding; no blanket NoPadding proof is granted.
template <class T>
struct alignas(128) Isolated {
  T value{};
  template <class Self>
  [[nodiscard]] constexpr decltype(auto) get(this Self&& self) noexcept {
    return std::forward_like<Self>(self.value);
  }
  template <class Self>
  [[nodiscard]] constexpr decltype(auto) operator*(this Self&& self) noexcept {
    return std::forward_like<Self>(self.value);
  }
  template <class Self>
  [[nodiscard]] constexpr auto data(this Self&& self) noexcept {
    if consteval {
      return std::addressof(self.value);
    } else {
      return std::assume_aligned<128>(std::addressof(self.value));
    }
  }
  template <class Self>
  [[nodiscard]] constexpr auto operator->(this Self&& self) noexcept {
    return self.data();
  }
  constexpr auto operator<=>(const Isolated&) const = default;
};
}  // namespace uarch::layout
