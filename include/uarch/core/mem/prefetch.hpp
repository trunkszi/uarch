#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <uarch/core/layout/register_passable.hpp>
#include <utility>

namespace uarch::mem {
/// Cache retention hint; no hint establishes memory ordering or ownership.
enum class Locality : int { None = 0, Low = 1, Moderate = 2, High = 3 };
inline constexpr Locality L1 = Locality::High;
inline constexpr Locality L2 = Locality::Moderate;
inline constexpr Locality L3 = Locality::Low;
inline constexpr Locality NonTemporal = Locality::None;

/// Permission for write-intent hints on privately owned cache lines.
class PrefetchWrite {
  constexpr explicit PrefetchWrite(int) noexcept {}

 public:
  PrefetchWrite() = delete ("obtain PrefetchWrite from uarch::detect::detect_prefetch_write() or assume_supported");
  constexpr PrefetchWrite(const PrefetchWrite&) noexcept = default;
  constexpr PrefetchWrite(PrefetchWrite&&) noexcept = default;
  constexpr PrefetchWrite& operator=(const PrefetchWrite& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr PrefetchWrite& operator=(PrefetchWrite&& other) noexcept {
    if (this != &other) static_cast<void>(*this == other);
    return *this;
  }
  constexpr ~PrefetchWrite() noexcept = default;
  constexpr bool operator==(const PrefetchWrite&) const noexcept = default;
  /// Caller must prove write-prefetch ISA support on every possible execution CPU.
  [[nodiscard]] static constexpr PrefetchWrite assume_supported() noexcept { return PrefetchWrite(0); }
};

static_assert(!std::is_trivially_copyable_v<PrefetchWrite> && layout::ZeroRegisterCapability<PrefetchWrite>);

/// Hints at a live reference without changing its lifetime or access permissions.
template <Locality L = Locality::High, class T>
constexpr void prefetch_read(const T& ref) noexcept {
  static_assert(std::to_underlying(L) >= 0 && std::to_underlying(L) <= 3, std::string_view("invalid prefetch locality"));
  if consteval {
  } else {
    // The locality operand must be an immediate at every optimization level.
    __builtin_prefetch(std::addressof(ref), 0, static_cast<int>(L));
  }
}

/// The capability must cover the ISA and the referenced line must be privately owned.
template <Locality L = Locality::High, class T>
constexpr void prefetch_write(const PrefetchWrite&, const T& ref) noexcept {
  static_assert(std::to_underlying(L) >= 0 && std::to_underlying(L) <= 3, std::string_view("invalid prefetch locality"));
  if consteval {
  } else {
#if defined(__x86_64__)
    // The capability permits PREFETCHW independently of translation-unit ISA flags.
    const auto* bytes = reinterpret_cast<const char*>(std::addressof(ref));
    asm volatile("prefetchw %0" : : "m"(*bytes));
#else
    __builtin_prefetch(std::addressof(ref), 1, static_cast<int>(L));
#endif
  }
}

/// Returns the offset in a 64-byte cache line; constant evaluation has no address.
template <class T>
[[nodiscard]] constexpr std::size_t cache_line_offset(const T* ptr) noexcept {
  if consteval {
    return 0uz;
  } else {
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr) & 63uz);
  }
}
}  // namespace uarch::mem
