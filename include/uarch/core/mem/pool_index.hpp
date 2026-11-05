/** @file @brief Nonzero physical slots and checked or explicit masked pool access. */
#pragma once
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <uarch/core/layout/register_passable.hpp>

namespace uarch::mem {
/// A physical slot, independent of any particular pool capacity or generation.
template <class Tag>
class[[= uarch::layout::register_leaf]] PoolIndex {
  std::uint32_t raw_;
  constexpr explicit PoolIndex(std::uint32_t raw) noexcept : raw_(raw) {}

 public:
  PoolIndex() = delete ("slot 0 is the pool sentinel; use PoolIndex::try_new");
  ~PoolIndex() = default;
  [[nodiscard, gnu::always_inline]] static constexpr std::optional<PoolIndex> try_new(std::uint32_t slot) noexcept {
    if (slot == 0) return std::nullopt;
    return PoolIndex(slot);
  }
  [[nodiscard, gnu::always_inline]] constexpr std::uint32_t slot() const noexcept { return raw_; }
  constexpr auto operator<=>(const PoolIndex&) const noexcept = default;
};

/// Four-byte nullable slot encoding; zero is a valid empty representation here.
template <class Tag>
class[[= uarch::layout::register_leaf]] OptionalPoolIndex {
  std::uint32_t raw_{};
  constexpr explicit OptionalPoolIndex(std::uint32_t raw) noexcept : raw_(raw) {}

 public:
  constexpr OptionalPoolIndex() noexcept = default;
  [[nodiscard, gnu::always_inline]] static constexpr OptionalPoolIndex none() noexcept { return {}; }
  [[nodiscard, gnu::always_inline]] static constexpr OptionalPoolIndex some(PoolIndex<Tag> index) noexcept {
    return OptionalPoolIndex(index.slot());
  }
  [[nodiscard, gnu::always_inline]] constexpr bool has_value() const noexcept { return raw_ != 0; }
  [[nodiscard, gnu::always_inline]] constexpr std::optional<PoolIndex<Tag>> get() const noexcept {
    return PoolIndex<Tag>::try_new(raw_);
  }
  constexpr auto operator<=>(const OptionalPoolIndex&) const noexcept = default;
};

/// Borrows exactly 2^Log initialized objects; slot zero remains an initialized sentinel.
/// The owner must keep the span's objects alive for every access through this view.
template <class T, unsigned Log>
class Pool {
  static_assert(Log >= 1 && Log <= 31, std::string_view("pool Log must be in 1..31"));
  std::span<T> slots_{};
  constexpr explicit Pool(std::span<T> slots) noexcept : slots_(slots) {}
  static constexpr std::size_t capacity = 1uz << Log;

 public:
  constexpr Pool(const Pool&) noexcept = default;
  constexpr Pool(Pool&&) noexcept = default;
  constexpr Pool& operator=(const Pool& other) noexcept {
    if (this != &other) slots_ = other.slots_;
    return *this;
  }
  constexpr Pool& operator=(Pool&& other) noexcept {
    if (this != &other) slots_ = other.slots_;
    return *this;
  }
  constexpr ~Pool() noexcept = default;
  [[nodiscard, gnu::always_inline]] static constexpr std::optional<Pool> try_new(std::span<T> slots) noexcept {
    if (slots.size() != capacity) return std::nullopt;
    return Pool(slots);
  }
  /// Sentinel and out-of-capacity representations have no checked element.
  [[nodiscard, gnu::always_inline]] constexpr T* get(PoolIndex<T> index) noexcept {
    const auto slot = static_cast<std::size_t>(index.slot());
    return (slot - 1uz) < (capacity - 1uz) ? &slots_[slot] : nullptr;
  }
  [[nodiscard, gnu::always_inline]] constexpr const T* get(PoolIndex<T> index) const noexcept {
    const auto slot = static_cast<std::size_t>(index.slot());
    return (slot - 1uz) < (capacity - 1uz) ? &slots_[slot] : nullptr;
  }
  /// Explicit ring access can alias the sentinel at physical slot zero.
  [[nodiscard, gnu::always_inline]] constexpr T& get_masked(PoolIndex<T> index) noexcept {
    return slots_[static_cast<std::size_t>(index.slot()) & (capacity - 1)];
  }
  [[nodiscard, gnu::always_inline]] constexpr const T& get_masked(PoolIndex<T> index) const noexcept {
    return slots_[static_cast<std::size_t>(index.slot()) & (capacity - 1)];
  }
};
}  // namespace uarch::mem
