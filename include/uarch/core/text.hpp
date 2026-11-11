/** @file @brief Canonical inline ASCII storage with register-sized word comparisons. */
#pragma once
#include <array>
#include <bit>
#include <compare>
#include <cstring>
#include <expected>
#include <uarch/core/hash.hpp>
namespace uarch::text {
enum class AsciiError : std::uint8_t { TooLong, EmbeddedNul, NonAscii };
namespace detail {
constexpr unsigned char ascii_byte(char c) noexcept { return static_cast<unsigned char>(c); }
constexpr unsigned char ascii_byte(std::uint8_t c) noexcept { return c; }
constexpr unsigned char ascii_byte(std::byte c) noexcept { return std::to_integer<unsigned char>(c); }
}  // namespace detail
template <std::size_t N>
  requires(N > 0)
struct FixedAscii {
  std::array<char, N> bytes{};
  static constexpr std::size_t kCapacity = N;
  [[nodiscard]] static constexpr FixedAscii empty() noexcept { return {}; }

 private:
  template <class Input>
  [[nodiscard]] static constexpr std::expected<FixedAscii, AsciiError> checked(Input input) noexcept {
    if (input.size() > N) return std::unexpected(AsciiError::TooLong);
    FixedAscii result;
    for (std::size_t i = 0; i < input.size(); ++i) {
      const auto c = detail::ascii_byte(input[i]);
      if (c == 0) return std::unexpected(AsciiError::EmbeddedNul);
      if (c > 127) return std::unexpected(AsciiError::NonAscii);
      result.bytes[i] = static_cast<char>(c);
    }
    return result;
  }
  template <class Input>
  [[nodiscard]] static constexpr FixedAscii truncated(Input input) noexcept {
    FixedAscii result;
    for (std::size_t i = 0; i < N && i < input.size(); ++i) {
      const auto c = detail::ascii_byte(input[i]);
      if (c == 0 || c > 127) break;
      result.bytes[i] = static_cast<char>(c);
    }
    return result;
  }
  [[nodiscard, gnu::always_inline]] static std::uint64_t word(const char* data, std::size_t count) noexcept {
    std::uint64_t value{};
    std::memcpy(&value, data, count);
    return value;
  }
  [[nodiscard, gnu::always_inline]] static std::uint64_t big_word(const char* data, std::size_t count) noexcept {
    const auto value = word(data, count);
    if constexpr (std::endian::native == std::endian::little)
      return std::byteswap(value);
    else
      return value;
  }

 public:
  [[nodiscard]] static constexpr std::expected<FixedAscii, AsciiError> try_from(std::span<const std::uint8_t> input) noexcept {
    return checked(input);
  }
  [[nodiscard]] static constexpr FixedAscii from_truncating(std::span<const std::uint8_t> input) noexcept {
    return truncated(input);
  }
  [[nodiscard]] static constexpr std::expected<FixedAscii, AsciiError> try_from(std::span<const std::byte> input) noexcept {
    return checked(input);
  }
  [[nodiscard]] static constexpr FixedAscii from_truncating(std::span<const std::byte> input) noexcept {
    return truncated(input);
  }
  [[nodiscard]] static constexpr std::expected<FixedAscii, AsciiError> try_from(std::string_view input) noexcept {
    return checked(input);
  }
  [[nodiscard]] static constexpr FixedAscii from_truncating(std::string_view input) noexcept { return truncated(input); }
  [[nodiscard]] static constexpr std::size_t capacity() noexcept { return N; }
  [[nodiscard]] constexpr std::size_t size() const noexcept {
    std::size_t length = 0;
    while (length < N && bytes[length] != 0) ++length;
    return length;
  }
  [[nodiscard]] constexpr bool is_empty() const noexcept { return bytes[0] == 0; }
  [[nodiscard]] constexpr std::span<const char, N> as_bytes() const noexcept { return bytes; }
  [[nodiscard]] constexpr std::span<const char> as_trimmed_bytes() const noexcept { return {bytes.data(), size()}; }
  [[nodiscard]] constexpr std::string_view as_string_view() const noexcept { return {bytes.data(), size()}; }
  [[nodiscard]] constexpr std::uint64_t fnv1a64() const noexcept { return hash::fnv1a64(as_string_view()); }
  [[nodiscard, gnu::always_inline]] friend constexpr bool operator==(const FixedAscii& a, const FixedAscii& b) noexcept {
    if !consteval {
      if constexpr (N <= 8)
        return word(a.bytes.data(), N) == word(b.bytes.data(), N);
      else if constexpr (N <= 16) {
        return ((word(a.bytes.data(), 8) ^ word(b.bytes.data(), 8)) |
                (word(a.bytes.data() + 8, N - 8) ^ word(b.bytes.data() + 8, N - 8))) == 0;
      }
    }
    for (std::size_t i = 0; i < N; ++i)
      if (a.bytes[i] != b.bytes[i]) return false;
    return true;
  }
  [[nodiscard, gnu::always_inline]] friend constexpr std::strong_ordering operator<=>(const FixedAscii& a,
                                                                                      const FixedAscii& b) noexcept {
    if !consteval {
      if constexpr (N <= 8)
        return big_word(a.bytes.data(), N) <=> big_word(b.bytes.data(), N);
      else if constexpr (N <= 16) {
        const auto a_first = word(a.bytes.data(), 8), b_first = word(b.bytes.data(), 8);
        const auto a_last = word(a.bytes.data() + 8, N - 8), b_last = word(b.bytes.data() + 8, N - 8);
        const bool equal_first = a_first == b_first;
        auto a_word = equal_first ? a_last : a_first;
        auto b_word = equal_first ? b_last : b_first;
        if constexpr (std::endian::native == std::endian::little) {
          a_word = std::byteswap(a_word);
          b_word = std::byteswap(b_word);
        }
        return a_word <=> b_word;
      }
    }
    for (std::size_t i = 0; i < N; ++i) {
      auto order = static_cast<unsigned char>(a.bytes[i]) <=> static_cast<unsigned char>(b.bytes[i]);
      if (order != 0) return order;
    }
    return std::strong_ordering::equal;
  }
};
}  // namespace uarch::text
