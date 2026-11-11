#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace uarch::arithmetic {
/// Compile-time division, remainder and overflow-checked alignment boundary.
template <unsigned Log>
struct PowerOfTwo {
  static_assert(Log < 64, std::string_view("power-of-two shift must be < 64"));
  static constexpr std::uint64_t value = std::uint64_t{1} << Log;
  static constexpr std::uint64_t mask = value - 1;
  [[nodiscard, gnu::always_inline]] static constexpr std::uint64_t div(std::uint64_t n) noexcept { return n >> Log; }
  [[nodiscard, gnu::always_inline]] static constexpr std::uint64_t rem(std::uint64_t n) noexcept { return n & mask; }
  [[nodiscard, gnu::always_inline]] static constexpr bool is_multiple_of(std::uint64_t n) noexcept { return rem(n) == 0; }
  [[nodiscard, gnu::always_inline]] static constexpr std::uint64_t align_down(std::uint64_t n) noexcept { return n & ~mask; }
  [[nodiscard, gnu::always_inline]] static constexpr std::optional<std::uint64_t> checked_align_up(std::uint64_t n) noexcept {
    const auto offset = (0ULL - n) & mask;
    if (n > std::numeric_limits<std::uint64_t>::max() - offset) return std::nullopt;
    return n + offset;
  }
  [[nodiscard, gnu::always_inline]] static constexpr std::uint64_t alignment_offset(std::uint64_t n) noexcept {
    return (0ULL - n) & mask;
  }
};
}  // namespace uarch::arithmetic
