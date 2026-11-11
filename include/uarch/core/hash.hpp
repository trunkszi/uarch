/** @file @brief Unsigned-byte FNV-1a hashing with constant-evaluated literals. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace uarch::hash {
inline constexpr std::uint64_t kFnv1a64OffsetBasis = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t kFnv1a64Prime = 0x100000001b3ULL;
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64_extend(std::uint64_t hash,
                                                                         std::span<const std::uint8_t> bytes) noexcept {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    hash ^= bytes[i];
    hash *= kFnv1a64Prime;
  }
  return hash;
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes) noexcept {
  return fnv1a64_extend(kFnv1a64OffsetBasis, bytes);
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64_extend(std::uint64_t hash,
                                                                         std::span<const std::byte> bytes) noexcept {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    hash ^= std::to_integer<std::uint8_t>(bytes[i]);
    hash *= kFnv1a64Prime;
  }
  return hash;
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
  return fnv1a64_extend(kFnv1a64OffsetBasis, bytes);
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64_extend(std::uint64_t hash, std::string_view bytes) noexcept {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    hash ^= static_cast<unsigned char>(bytes[i]);
    hash *= kFnv1a64Prime;
  }
  return hash;
}
[[nodiscard, gnu::always_inline]] constexpr std::uint64_t fnv1a64(std::string_view bytes) noexcept {
  return fnv1a64_extend(kFnv1a64OffsetBasis, bytes);
}
template <std::size_t N>
  requires(N > 0)
[[nodiscard]] consteval std::uint64_t fnv1a64_literal(const char (&literal)[N]) noexcept {
  return fnv1a64(std::string_view{literal, N - 1});
}
}  // namespace uarch::hash
