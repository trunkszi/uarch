/** @file @brief Scalar bitmap operations with defined empty and full-width boundaries. */
#pragma once

#include <bit>
#include <bitset>
#include <compare>
#include <cstdlib>
#include <expected>
#include <optional>
#include <string_view>
#include <uarch/core/bits/bit_value.hpp>

namespace uarch::bits {
/** @brief A padding-free unsigned word with ascending-index bit operations. */
class Bitmap64 {
  std::uint64_t bits_{};
  explicit constexpr Bitmap64(std::uint64_t raw) noexcept : bits_(raw) {}
  static constexpr void CheckIndex(std::uint32_t bit) noexcept {
    if (bit >= 64) std::abort();
  }

 public:
  constexpr Bitmap64() noexcept = default;
  [[nodiscard, gnu::always_inline]] static constexpr Bitmap64 empty() noexcept { return Bitmap64{0}; }
  [[nodiscard, gnu::always_inline]] static constexpr Bitmap64 all() noexcept { return Bitmap64{~std::uint64_t{0}}; }
  [[nodiscard, gnu::always_inline]] static constexpr Bitmap64 from_raw(std::uint64_t raw) noexcept { return Bitmap64{raw}; }
  [[nodiscard, gnu::always_inline]] static constexpr Bitmap64 from_bitset(std::bitset<64> bits) noexcept {
    return Bitmap64{bits.to_ullong()};
  }
  /** @brief Reject invalid text before invoking the exception-capable standard bitset constructor. */
  [[nodiscard, gnu::always_inline]] static constexpr std::expected<Bitmap64, BitError> try_from_string(
      std::string_view text) noexcept {
    if (text.size() > 64) return std::unexpected(BitError::kInvalidString);
    for (char digit : text)
      if (digit != '0' && digit != '1') return std::unexpected(BitError::kInvalidString);
    return from_bitset(std::bitset<64>{text});
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t raw() const noexcept { return bits_; }
  [[nodiscard, gnu::always_inline]] constexpr std::bitset<64> to_bitset() const noexcept { return std::bitset<64>{bits_}; }
  [[nodiscard, gnu::always_inline]] constexpr std::uint32_t count() const noexcept {
    return static_cast<std::uint32_t>(std::popcount(bits_));
  }
  [[nodiscard, gnu::always_inline]] constexpr bool is_empty() const noexcept { return bits_ == 0; }
  [[nodiscard, gnu::always_inline]] constexpr bool is_full() const noexcept { return bits_ == ~std::uint64_t{0}; }
  /** @brief Index operations reject bit >=64 in every profile. */
  [[nodiscard, gnu::always_inline]] constexpr bool test(std::uint32_t bit) const noexcept {
    CheckIndex(bit);
    return (bits_ & (std::uint64_t{1} << bit)) != 0;
  }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 set(std::uint32_t bit) const noexcept {
    CheckIndex(bit);
    return Bitmap64{bits_ | (std::uint64_t{1} << bit)};
  }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 clear(std::uint32_t bit) const noexcept {
    CheckIndex(bit);
    return Bitmap64{bits_ & ~(std::uint64_t{1} << bit)};
  }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 toggle(std::uint32_t bit) const noexcept {
    CheckIndex(bit);
    return Bitmap64{bits_ ^ (std::uint64_t{1} << bit)};
  }
  /** @brief Lowest set bit index, or no value for zero. */
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> lowest_one() const noexcept {
    if (is_empty()) return std::nullopt;
    return static_cast<std::uint32_t>(std::countr_zero(bits_));
  }
  /** @brief Highest set bit index, or no value for zero. */
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> highest_one() const noexcept {
    if (is_empty()) return std::nullopt;
    return 63U - static_cast<std::uint32_t>(std::countl_zero(bits_));
  }
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> lowest_zero() const noexcept {
    return (~*this).lowest_one();
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t lowest_one_bit() const noexcept {
    return bits_ & (std::uint64_t{0} - bits_);
  }
  [[nodiscard, gnu::always_inline]] static constexpr std::uint64_t highest_one_bit(std::uint64_t word) noexcept {
    return std::bit_floor(word);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint64_t highest_one_bit() const noexcept { return highest_one_bit(bits_); }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 clear_lowest_one() const noexcept {
    return Bitmap64{bits_ & (bits_ - 1)};
  }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 clear_highest_one() const noexcept {
    return Bitmap64{bits_ ^ highest_one_bit()};
  }
  [[gnu::always_inline]] constexpr std::optional<std::uint32_t> pop_lowest_one() noexcept {
    auto index = lowest_one();
    *this = clear_lowest_one();
    return index;
  }
  [[gnu::always_inline]] constexpr std::optional<std::uint32_t> pop_highest_one() noexcept {
    auto index = highest_one();
    *this = clear_highest_one();
    return index;
  }
  /** @brief First set index at/above min; min >=64 returns no value. */
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> next_one_from(std::uint32_t min) const noexcept {
    if (min >= 64) return std::nullopt;
    return Bitmap64{bits_ & (~std::uint64_t{0} << min)}.lowest_one();
  }
  /** @brief Last set index at/below max; max >=64 searches the whole bitmap. */
  [[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> prev_one_from(std::uint32_t max) const noexcept {
    if (max >= 64) return highest_one();
    return Bitmap64{bits_ & (~std::uint64_t{0} >> (63U - max))}.highest_one();
  }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 union_with(Bitmap64 other) const noexcept { return *this | other; }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 intersection(Bitmap64 other) const noexcept { return *this & other; }
  [[nodiscard, gnu::always_inline]] constexpr Bitmap64 difference(Bitmap64 other) const noexcept { return *this - other; }
  constexpr auto operator<=>(const Bitmap64&) const noexcept = default;
  [[nodiscard, gnu::always_inline]] friend constexpr Bitmap64 operator&(Bitmap64 a, Bitmap64 b) noexcept {
    return Bitmap64{a.bits_ & b.bits_};
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Bitmap64 operator|(Bitmap64 a, Bitmap64 b) noexcept {
    return Bitmap64{a.bits_ | b.bits_};
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Bitmap64 operator^(Bitmap64 a, Bitmap64 b) noexcept {
    return Bitmap64{a.bits_ ^ b.bits_};
  }
  [[nodiscard, gnu::always_inline]] friend constexpr Bitmap64 operator~(Bitmap64 a) noexcept { return Bitmap64{~a.bits_}; }
  [[nodiscard, gnu::always_inline]] friend constexpr Bitmap64 operator-(Bitmap64 a, Bitmap64 b) noexcept {
    return Bitmap64{a.bits_ & ~b.bits_};
  }
  [[gnu::always_inline]] constexpr Bitmap64& operator&=(Bitmap64 other) noexcept {
    bits_ &= other.bits_;
    return *this;
  }
  [[gnu::always_inline]] constexpr Bitmap64& operator|=(Bitmap64 other) noexcept {
    bits_ |= other.bits_;
    return *this;
  }
  [[gnu::always_inline]] constexpr Bitmap64& operator^=(Bitmap64 other) noexcept {
    bits_ ^= other.bits_;
    return *this;
  }
};
}  // namespace uarch::bits
