/** @file @brief Bounded inline vector with fallible insertion and no runtime allocation. */
#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace uarch::container {
namespace detail {
template <class T>
inline constexpr bool kConstexprStorage = std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>;

/** @brief Array storage can propagate trivial special members without element lifetime loops. */
template <class T>
concept TrivialStorage = kConstexprStorage<T> && std::is_trivially_copyable_v<T>;

template <class T, std::size_t N, bool = kConstexprStorage<T>>
class InlineStorage {
  // A byte array provides storage for the implicit-lifetime backing array; T elements start individually.
  alignas(T) std::byte storage_[sizeof(T) * N]{};

 public:
  [[nodiscard, gnu::always_inline]] constexpr T* uninitialized_data() noexcept {
    return static_cast<T*>(static_cast<void*>(storage_));
  }
  [[nodiscard, gnu::always_inline]] constexpr const T* uninitialized_data() const noexcept {
    return static_cast<const T*>(static_cast<const void*>(storage_));
  }
  [[nodiscard, gnu::always_inline]] constexpr T* data() noexcept { return std::launder(uninitialized_data()); }
  [[nodiscard, gnu::always_inline]] constexpr const T* data() const noexcept { return std::launder(uninitialized_data()); }
};
template <class T, std::size_t N>
class InlineStorage<T, N, true> {
  std::array<T, N> storage_{};

 public:
  [[nodiscard, gnu::always_inline]] constexpr T* uninitialized_data() noexcept { return storage_.data(); }
  [[nodiscard, gnu::always_inline]] constexpr const T* uninitialized_data() const noexcept { return storage_.data(); }
  [[nodiscard, gnu::always_inline]] constexpr T* data() noexcept { return storage_.data(); }
  [[nodiscard, gnu::always_inline]] constexpr const T* data() const noexcept { return storage_.data(); }
};
}  // namespace detail

/** @brief Fixed capacity. Trivial elements support constant evaluation; non-trivial elements use runtime RAII.
 * Value-initialized arrays preserve constexpr operations and trivial copying. Copy/move of trivial storage
 * carries sizeof(inplace_vector<T, N>) bytes, including unused capacity, independently of size().
 */
template <class T, std::size_t N>
class inplace_vector {
  static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>);
  detail::InlineStorage<T, N> storage_{};
  std::size_t size_{};

 public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = T*;
  using const_iterator = const T*;
  constexpr inplace_vector() noexcept = default;
  constexpr inplace_vector(const inplace_vector&) noexcept
    requires detail::TrivialStorage<T>
  = default;
  constexpr inplace_vector(inplace_vector&&) noexcept
    requires detail::TrivialStorage<T>
  = default;
  constexpr inplace_vector& operator=(const inplace_vector&) noexcept
    requires detail::TrivialStorage<T>
  = default;
  constexpr inplace_vector& operator=(inplace_vector&&) noexcept
    requires detail::TrivialStorage<T>
  = default;
  constexpr ~inplace_vector() noexcept
    requires detail::TrivialStorage<T>
  = default;

  constexpr inplace_vector(const inplace_vector& other) noexcept
    requires(!detail::TrivialStorage<T> && std::is_copy_constructible_v<T>)
  {
    for (const auto& value : other) unchecked_emplace_back(value);
  }
  constexpr inplace_vector(inplace_vector&& other) noexcept
    requires(!detail::TrivialStorage<T> && std::is_move_constructible_v<T>)
  {
    for (auto& value : other) unchecked_emplace_back(std::move(value));
  }
  constexpr inplace_vector& operator=(const inplace_vector& other) noexcept
    requires(!detail::TrivialStorage<T> && std::is_copy_constructible_v<T>)
  {
    if (this != &other) {
      clear();
      for (const auto& value : other) unchecked_emplace_back(value);
    }
    return *this;
  }
  constexpr inplace_vector& operator=(inplace_vector&& other) noexcept
    requires(!detail::TrivialStorage<T> && std::is_move_constructible_v<T>)
  {
    if (this != &other) {
      clear();
      for (auto& value : other) unchecked_emplace_back(std::move(value));
    }
    return *this;
  }
  constexpr ~inplace_vector() noexcept
    requires(!detail::TrivialStorage<T>)
  {
    clear();
  }

  /** @brief Capacity failure leaves all existing elements untouched; T construction must not throw. */
  template <class... Args>
    requires std::constructible_from<T, Args...>
  [[gnu::always_inline]] constexpr T* try_emplace_back(Args&&... args) noexcept {
    if (full()) return nullptr;
    return std::addressof(unchecked_emplace_back(std::forward<Args>(args)...));
  }
  [[gnu::always_inline]] constexpr T* try_push_back(const T& value) noexcept
    requires std::is_copy_constructible_v<T>
  {
    return try_emplace_back(value);
  }
  [[gnu::always_inline]] constexpr T* try_push_back(T&& value) noexcept
    requires std::is_move_constructible_v<T>
  {
    return try_emplace_back(std::move(value));
  }
  /** @brief Precondition: size() < N. T construction must not throw. */
  template <class... Args>
    requires std::constructible_from<T, Args...>
  [[gnu::always_inline]] constexpr T& unchecked_emplace_back(Args&&... args) noexcept {
    [[assume(size_ < N)]];
    T* result = std::construct_at(storage_.uninitialized_data() + size_, std::forward<Args>(args)...);
    ++size_;
    return *result;
  }
  [[gnu::always_inline]] constexpr T& unchecked_push_back(const T& value) noexcept
    requires std::is_copy_constructible_v<T>
  {
    return unchecked_emplace_back(value);
  }
  [[gnu::always_inline]] constexpr T& unchecked_push_back(T&& value) noexcept
    requires std::is_move_constructible_v<T>
  {
    return unchecked_emplace_back(std::move(value));
  }
  void push_back(const T&) =
      delete ("unbounded push_back is banned under -fno-exceptions; use try_push_back or unchecked_push_back");
  void push_back(T&&) =
      delete ("unbounded push_back is banned under -fno-exceptions; use try_push_back or unchecked_push_back");
  template <class... Args>
  void emplace_back(Args&&...) =
      delete ("unbounded emplace_back is banned under -fno-exceptions; use try_emplace_back or unchecked_emplace_back");

  // Launder only live objects; empty storage is an address for placement construction, not a T yet.
  [[nodiscard, gnu::always_inline]] constexpr T* data() noexcept {
    return empty() ? storage_.uninitialized_data() : storage_.data();
  }
  [[nodiscard, gnu::always_inline]] constexpr const T* data() const noexcept {
    return empty() ? storage_.uninitialized_data() : storage_.data();
  }
  [[nodiscard, gnu::always_inline]] constexpr iterator begin() noexcept { return data(); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator begin() const noexcept { return data(); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cbegin() const noexcept { return data(); }
  [[nodiscard, gnu::always_inline]] constexpr iterator end() noexcept { return data() + size_; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator end() const noexcept { return data() + size_; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cend() const noexcept { return end(); }
  [[nodiscard, gnu::always_inline]] constexpr size_type size() const noexcept { return size_; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type capacity() noexcept { return N; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type max_size() noexcept { return N; }
  [[nodiscard, gnu::always_inline]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard, gnu::always_inline]] constexpr bool full() const noexcept { return size_ == N; }
  [[nodiscard, gnu::always_inline]] constexpr T& operator[](size_type index) noexcept {
    if (index >= size_) std::abort();
    return data()[index];
  }
  [[nodiscard, gnu::always_inline]] constexpr const T& operator[](size_type index) const noexcept {
    if (index >= size_) std::abort();
    return data()[index];
  }
  [[nodiscard, gnu::always_inline]] constexpr T& front() noexcept { return (*this)[0]; }
  [[nodiscard, gnu::always_inline]] constexpr const T& front() const noexcept { return (*this)[0]; }
  [[nodiscard, gnu::always_inline]] constexpr T& back() noexcept {
    if (empty()) std::abort();
    return (*this)[size_ - 1];
  }
  [[nodiscard, gnu::always_inline]] constexpr const T& back() const noexcept {
    if (empty()) std::abort();
    return (*this)[size_ - 1];
  }
  /** @brief Destroy the final live element; empty vectors are rejected in every build profile. */
  [[gnu::always_inline]] constexpr void pop_back() noexcept {
    if (empty()) std::abort();
    --size_;
    if constexpr (!std::is_trivially_destructible_v<T>) std::destroy_at(data() + size_);
  }
  [[gnu::always_inline]] constexpr void clear() noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
      size_ = 0;
    } else {
      while (!empty()) pop_back();
    }
  }
  [[nodiscard, gnu::always_inline]] constexpr bool operator==(const inplace_vector& other) const noexcept
    requires std::equality_comparable<T>
  {
    return size_ == other.size_ && std::equal(begin(), end(), other.begin());
  }
  [[nodiscard, gnu::always_inline]] constexpr auto operator<=>(const inplace_vector& other) const noexcept
    requires std::three_way_comparable<T>
  {
    return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
  }
};

/** @brief Empty capacity has no T objects or storage and is usable in constant expressions for any T. */
template <class T>
class inplace_vector<T, 0> {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = T*;
  using const_iterator = const T*;
  constexpr inplace_vector() noexcept = default;
  [[nodiscard, gnu::always_inline]] constexpr T* data() noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr const T* data() const noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr iterator begin() noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator begin() const noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cbegin() const noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr iterator end() noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator end() const noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cend() const noexcept { return nullptr; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type size() noexcept { return 0; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type capacity() noexcept { return 0; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type max_size() noexcept { return 0; }
  [[nodiscard, gnu::always_inline]] static constexpr bool empty() noexcept { return true; }
  [[nodiscard, gnu::always_inline]] static constexpr bool full() noexcept { return true; }
  [[gnu::always_inline]] constexpr T* try_push_back(const T&) noexcept { return nullptr; }
  [[gnu::always_inline]] constexpr T* try_push_back(T&&) noexcept { return nullptr; }
  template <class... Args>
  [[gnu::always_inline]] constexpr T* try_emplace_back(Args&&...) noexcept {
    return nullptr;
  }
  template <class... Args>
  [[gnu::always_inline]] constexpr T& unchecked_emplace_back(Args&&...) noexcept {
    [[assume(false)]];
    std::abort();
  }
  [[gnu::always_inline]] constexpr T& unchecked_push_back(const T&) noexcept {
    [[assume(false)]];
    std::abort();
  }
  [[gnu::always_inline]] constexpr T& unchecked_push_back(T&&) noexcept {
    [[assume(false)]];
    std::abort();
  }
  void push_back(const T&) =
      delete ("unbounded push_back is banned under -fno-exceptions; use try_push_back or unchecked_push_back");
  void push_back(T&&) =
      delete ("unbounded push_back is banned under -fno-exceptions; use try_push_back or unchecked_push_back");
  template <class... Args>
  void emplace_back(Args&&...) =
      delete ("unbounded emplace_back is banned under -fno-exceptions; use try_emplace_back or unchecked_emplace_back");
  [[nodiscard, gnu::always_inline]] constexpr T& operator[](size_type) noexcept { std::abort(); }
  [[nodiscard, gnu::always_inline]] constexpr const T& operator[](size_type) const noexcept { std::abort(); }
  [[nodiscard, gnu::always_inline]] constexpr T& front() noexcept { std::abort(); }
  [[nodiscard, gnu::always_inline]] constexpr const T& front() const noexcept { std::abort(); }
  [[nodiscard, gnu::always_inline]] constexpr T& back() noexcept { std::abort(); }
  [[nodiscard, gnu::always_inline]] constexpr const T& back() const noexcept { std::abort(); }
  [[gnu::always_inline]] constexpr void pop_back() noexcept { std::abort(); }
  [[gnu::always_inline]] constexpr void clear() noexcept {}
  constexpr bool operator==(const inplace_vector&) const noexcept = default;
  constexpr std::strong_ordering operator<=>(const inplace_vector&) const noexcept { return std::strong_ordering::equal; }
};
}  // namespace uarch::container
