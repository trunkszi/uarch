/** @file @brief Inline fixed-capacity sequences with checked, fallible and unchecked insertion. */
#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <type_traits>
#include <utility>

namespace uarch::container {
namespace detail {
template <class T>
inline constexpr bool kConstexprStorage = std::is_nothrow_default_constructible_v<T> && std::is_trivially_destructible_v<T>;

template <std::size_t N>
using smallest_size_t = std::conditional_t<
    (N <= UINT8_MAX), std::uint8_t,
    std::conditional_t<(N <= UINT16_MAX), std::uint16_t, std::conditional_t<(N <= UINT32_MAX), std::uint32_t, std::size_t>>>;

/** @brief No data or element operations are needed for an empty capacity. */
template <class T>
struct zero_sized {
  [[nodiscard]] static constexpr T* storage_data() noexcept { return nullptr; }
  [[nodiscard]] static constexpr std::size_t storage_size() noexcept { return 0; }
  static constexpr void set_storage_size(std::size_t) noexcept {}
};
/** @brief Live array storage permits constant evaluation and preserves element copy traits. */
template <class T, std::size_t N>
struct trivial {
  // Parenthesized value initialization admits explicit default constructors as well.
  std::array<T, N> storage_data_ = std::array<T, N>();
  smallest_size_t<N> storage_size_{0};
  [[nodiscard]] constexpr T* storage_data() noexcept { return storage_data_.data(); }
  [[nodiscard]] constexpr const T* storage_data() const noexcept { return storage_data_.data(); }
  [[nodiscard]] constexpr std::size_t storage_size() const noexcept { return storage_size_; }
  constexpr void set_storage_size(std::size_t count) noexcept { storage_size_ = static_cast<smallest_size_t<N>>(count); }
};
/** @brief Raw slots avoid default construction and zeroing of inactive non-trivial elements. */
template <class T, std::size_t N>
struct non_trivial {
  alignas(T) std::byte storage_data_[sizeof(T) * N];
  smallest_size_t<N> storage_size_;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) -- inactive slots must remain uninitialized.
  non_trivial() noexcept : storage_size_(0) {}
  [[nodiscard]] T* storage_data() noexcept { return reinterpret_cast<T*>(storage_data_); }
  [[nodiscard]] const T* storage_data() const noexcept { return reinterpret_cast<const T*>(storage_data_); }
  [[nodiscard]] constexpr std::size_t storage_size() const noexcept { return storage_size_; }
  constexpr void set_storage_size(std::size_t count) noexcept { storage_size_ = static_cast<smallest_size_t<N>>(count); }
};
template <class T, std::size_t N>
using storage_for = std::conditional_t<N == 0, zero_sized<T>,
                                       std::conditional_t<detail::kConstexprStorage<T>, trivial<T, N>, non_trivial<T, N>>>;
struct freestanding_deleted {};
}  // namespace detail

/** @brief Runtime-free sequence. Checked operations abort on a violated bound; fallible insertion preserves capacity. */
template <class T, std::size_t N>
class inplace_vector : private detail::storage_for<T, N> {
  static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>);
  using Storage = detail::storage_for<T, N>;
  static constexpr bool kCopyAssignTrivial =
      N == 0 || (std::is_trivially_destructible_v<T> && std::is_trivially_copy_constructible_v<T> &&
                 std::is_trivially_copy_assignable_v<T>);
  static constexpr bool kMoveAssignTrivial =
      N == 0 || (std::is_trivially_destructible_v<T> && std::is_trivially_move_constructible_v<T> &&
                 std::is_trivially_move_assignable_v<T>);

 public:
  using value_type = T;
  using allocator_type = void;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr inplace_vector() noexcept
    requires(N == 0 || detail::kConstexprStorage<T>)
  = default;
  // NOLINTNEXTLINE(modernize-use-equals-default) -- user-provided construction avoids value-initializing raw slots.
  constexpr inplace_vector() noexcept
    requires(N > 0 && !detail::kConstexprStorage<T>)
  {}
  constexpr explicit inplace_vector(size_type count) noexcept { resize(count); }
  constexpr inplace_vector(size_type count, const T& value) noexcept { assign(count, value); }
  template <std::input_iterator InputIt>
  constexpr inplace_vector(InputIt first, InputIt last) noexcept {
    assign(first, last);
  }
  template <std::ranges::input_range R>
  constexpr inplace_vector(std::from_range_t, R&& range) noexcept {
    append_range(std::forward<R>(range));
  }
  constexpr inplace_vector(std::initializer_list<T> values) noexcept { append_range(values); }

  constexpr inplace_vector(const inplace_vector&) noexcept
    requires(N == 0 || std::is_trivially_copy_constructible_v<T>)
  = default;
  constexpr inplace_vector(const inplace_vector& other) noexcept
    requires(N > 0 && !std::is_trivially_copy_constructible_v<T> && std::is_copy_constructible_v<T>)
  {
    for (const auto& value : other) unchecked_emplace_back(value);
  }
  constexpr inplace_vector(inplace_vector&&) noexcept
    requires(N == 0 || std::is_trivially_move_constructible_v<T>)
  = default;
  constexpr inplace_vector(inplace_vector&& other) noexcept
    requires(N > 0 && !std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>)
  {
    for (auto& value : other) unchecked_emplace_back(std::move(value));
  }
  constexpr inplace_vector& operator=(const inplace_vector&) noexcept
    requires kCopyAssignTrivial
  = default;
  constexpr inplace_vector& operator=(const inplace_vector& other) noexcept
    requires(!kCopyAssignTrivial && std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>)
  {
    if (this != &other) assign(other.begin(), other.end());
    return *this;
  }
  constexpr inplace_vector& operator=(inplace_vector&&) noexcept
    requires kMoveAssignTrivial
  = default;
  constexpr inplace_vector& operator=(inplace_vector&& other) noexcept
    requires(!kMoveAssignTrivial && std::is_move_constructible_v<T> && std::is_move_assignable_v<T>)
  {
    if (this != &other) {
      clear();
      for (auto& value : other) unchecked_emplace_back(std::move(value));
    }
    return *this;
  }
  constexpr ~inplace_vector() noexcept
    requires(N == 0 || std::is_trivially_destructible_v<T>)
  = default;
  constexpr ~inplace_vector() noexcept
    requires(N > 0 && !std::is_trivially_destructible_v<T>)
  {
    clear();
  }
  constexpr inplace_vector& operator=(std::initializer_list<T> values) noexcept {
    assign(values);
    return *this;
  }

  template <std::input_iterator InputIt>
  constexpr void assign(InputIt first, InputIt last) noexcept {
    clear();
    for (; first != last; ++first) emplace_back(*first);
  }
  template <std::ranges::input_range R>
  constexpr void assign_range(R&& range) noexcept {
    clear();
    append_range(std::forward<R>(range));
  }
  constexpr void assign(size_type count, const T& value) noexcept {
    if (count > N) std::abort();
    if (count == 0) {
      clear();
      return;
    }
    // Snapshot before destroying existing elements: value may alias this sequence.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) -- value may alias an element moved or destroyed below.
    const T snapshot(value);
    clear();
    for (size_type i = 0; i < count; ++i) unchecked_emplace_back(snapshot);
  }
  constexpr void assign(std::initializer_list<T> values) noexcept { assign(values.begin(), values.end()); }

  [[nodiscard, gnu::always_inline]] constexpr pointer data() noexcept {
    if constexpr (N == 0 || detail::kConstexprStorage<T>)
      return this->storage_data();
    else
      return empty() ? this->storage_data() : std::launder(this->storage_data());
  }
  [[nodiscard, gnu::always_inline]] constexpr const_pointer data() const noexcept {
    if constexpr (N == 0 || detail::kConstexprStorage<T>)
      return this->storage_data();
    else
      return empty() ? this->storage_data() : std::launder(this->storage_data());
  }
  [[nodiscard, gnu::always_inline]] constexpr iterator begin() noexcept { return data(); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator begin() const noexcept { return data(); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard, gnu::always_inline]] constexpr iterator end() noexcept { return offset(size()); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator end() const noexcept { return offset(size()); }
  [[nodiscard, gnu::always_inline]] constexpr const_iterator cend() const noexcept { return end(); }
  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator{end()}; }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator{end()}; }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator{begin()}; }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator{begin()}; }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }
  [[nodiscard, gnu::always_inline]] constexpr size_type size() const noexcept { return this->storage_size(); }
  [[nodiscard, gnu::always_inline]] static constexpr size_type capacity() noexcept { return N; }
  [[nodiscard, gnu::always_inline]] static constexpr size_type max_size() noexcept { return N; }
  [[nodiscard, gnu::always_inline]] constexpr bool empty() const noexcept { return size() == 0; }
  [[nodiscard, gnu::always_inline]] constexpr bool full() const noexcept { return size() == N; }
  [[nodiscard, gnu::always_inline]] constexpr reference operator[](size_type index) noexcept {
    if (index >= size()) std::abort();
    return data()[index];
  }
  [[nodiscard, gnu::always_inline]] constexpr const_reference operator[](size_type index) const noexcept {
    if (index >= size()) std::abort();
    return data()[index];
  }
  [[nodiscard]] constexpr reference at(size_type index) noexcept { return (*this)[index]; }
  [[nodiscard]] constexpr const_reference at(size_type index) const noexcept { return (*this)[index]; }
  [[nodiscard, gnu::always_inline]] constexpr reference front() noexcept { return (*this)[0]; }
  [[nodiscard, gnu::always_inline]] constexpr const_reference front() const noexcept { return (*this)[0]; }
  [[nodiscard, gnu::always_inline]] constexpr reference back() noexcept {
    if (empty()) std::abort();
    return (*this)[size() - 1];
  }
  [[nodiscard, gnu::always_inline]] constexpr const_reference back() const noexcept {
    if (empty()) std::abort();
    return (*this)[size() - 1];
  }
  [[nodiscard]] constexpr bool try_resize(size_type count) noexcept
    requires std::default_initializable<T>
  {
    if (count > N) return false;
    while (size() > count) pop_back();
    while (size() < count) unchecked_emplace_back();
    return true;
  }
  [[nodiscard]] constexpr bool try_resize(size_type count, const T& value) noexcept
    requires std::copy_constructible<T>
  {
    if (count > N) return false;
    if (count <= size()) {
      while (size() > count) pop_back();
      return true;
    }
    // Preserve an aliased source across construction of new elements.
    const T snapshot(value);
    while (size() < count) unchecked_emplace_back(snapshot);
    return true;
  }
  constexpr void resize(size_type count) noexcept {
    if (count > N) std::abort();
    while (size() > count) pop_back();
    while (size() < count) unchecked_emplace_back();
  }
  constexpr void resize(size_type count, const T& value) noexcept {
    if (count > N) std::abort();
    while (size() > count) pop_back();
    while (size() < count) unchecked_emplace_back(value);
  }
  static constexpr void reserve(size_type count) noexcept {
    if (count > N) std::abort();
  }
  static constexpr void shrink_to_fit() noexcept {}

  template <class... Args>
    requires std::constructible_from<T, Args...>
  [[gnu::always_inline]] constexpr pointer try_emplace_back(Args&&... args) noexcept {
    if (full()) return nullptr;
    return std::addressof(unchecked_emplace_back(std::forward<Args>(args)...));
  }
  [[gnu::always_inline]] constexpr pointer try_push_back(const T& value) noexcept
    requires std::is_copy_constructible_v<T>
  {
    return try_emplace_back(value);
  }
  [[gnu::always_inline]] constexpr pointer try_push_back(T&& value) noexcept
    requires std::is_move_constructible_v<T>
  {
    return try_emplace_back(std::move(value));
  }
  template <class... Args>
    requires std::constructible_from<T, Args...>
  [[gnu::always_inline]] constexpr reference unchecked_emplace_back(Args&&... args) noexcept {
    const auto count = size();
    [[assume(count < N)]];
    if constexpr (N == 0)
      std::abort();
    else {
      auto* result = std::construct_at(this->storage_data() + size(), std::forward<Args>(args)...);
      this->set_storage_size(size() + 1);
      return *result;
    }
  }
  [[gnu::always_inline]] constexpr reference unchecked_push_back(const T& value) noexcept
    requires std::is_copy_constructible_v<T>
  {
    return unchecked_emplace_back(value);
  }
  [[gnu::always_inline]] constexpr reference unchecked_push_back(T&& value) noexcept
    requires std::is_move_constructible_v<T>
  {
    return unchecked_emplace_back(std::move(value));
  }
  template <class... Args>
    requires std::constructible_from<T, Args...>
  constexpr reference emplace_back(Args&&... args) noexcept {
    auto* inserted = try_emplace_back(std::forward<Args>(args)...);
    if (inserted == nullptr) std::abort();
    return *inserted;
  }
  constexpr reference push_back(const T& value) noexcept
    requires std::is_copy_constructible_v<T>
  {
    return emplace_back(value);
  }
  constexpr reference push_back(T&& value) noexcept
    requires std::is_move_constructible_v<T>
  {
    return emplace_back(std::move(value));
  }
  template <std::ranges::input_range R>
  constexpr std::ranges::borrowed_iterator_t<R> try_append_range(R&& range) noexcept {
    auto first = std::ranges::begin(range);
    const auto last = std::ranges::end(range);
    for (; !full() && first != last; ++first) unchecked_emplace_back(*first);
    if constexpr (std::ranges::borrowed_range<R>)
      return first;
    else
      return std::ranges::dangling{};
  }
  template <std::ranges::input_range R>
  constexpr void append_range(R&& range) noexcept {
    for (auto&& value : range) emplace_back(std::forward<decltype(value)>(value));
  }
  [[gnu::always_inline]] constexpr void pop_back() noexcept {
    if (empty()) std::abort();
    if constexpr (N > 0) {
      if constexpr (!std::is_trivially_destructible_v<T>) std::destroy_at(data() + size() - 1);
      this->set_storage_size(size() - 1);
    }
  }
  [[gnu::always_inline]] constexpr void clear() noexcept {
    if constexpr (N > 0) {
      if constexpr (!std::is_trivially_destructible_v<T>) std::destroy(begin(), end());
      this->set_storage_size(0);
    }
  }
  template <class... Args>
  constexpr iterator emplace(const_iterator position, Args&&... args) noexcept {
    const auto index = index_of(position);
    if (full()) std::abort();
    if (index == size()) {
      emplace_back(std::forward<Args>(args)...);
      return offset(index);
    }
    T snapshot(std::forward<Args>(args)...);
    const auto old_size = size();
    unchecked_emplace_back(std::move(back()));
    std::move_backward(offset(index), offset(old_size - 1), offset(old_size));
    (*this)[index] = std::move(snapshot);
    return offset(index);
  }
  constexpr iterator insert(const_iterator position, const T& value) noexcept { return emplace(position, value); }
  constexpr iterator insert(const_iterator position, T&& value) noexcept { return emplace(position, std::move(value)); }
  constexpr iterator insert(const_iterator position, size_type count, const T& value) noexcept {
    const auto index = index_of(position);
    if (count > N - size()) std::abort();
    if (count == 0) return offset(index);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) -- value may alias an element moved or destroyed below.
    const T snapshot(value);
    const auto old_size = size();
    for (size_type i = 0; i < count; ++i) unchecked_emplace_back(snapshot);
    std::rotate(offset(index), offset(old_size), end());
    return offset(index);
  }
  template <std::input_iterator InputIt>
  constexpr iterator insert(const_iterator position, InputIt first, InputIt last) noexcept {
    return insert_range(position, std::ranges::subrange(first, last));
  }
  constexpr iterator insert(const_iterator position, std::initializer_list<T> values) noexcept {
    return insert(position, values.begin(), values.end());
  }
  template <std::ranges::input_range R>
  constexpr iterator insert_range(const_iterator position, R&& range) noexcept {
    const auto index = index_of(position);
    const auto old_size = size();
    append_range(std::forward<R>(range));
    std::rotate(offset(index), offset(old_size), end());
    return offset(index);
  }
  constexpr iterator erase(const_iterator position) noexcept {
    const auto index = index_of(position);
    if (index == size()) std::abort();
    return erase(position, position + 1);
  }
  constexpr iterator erase(const_iterator first, const_iterator last) noexcept {
    const auto from = index_of(first), to = index_of(last);
    if (from > to) std::abort();
    if constexpr (N > 0)
      if (from != to) {
        std::move(offset(to), end(), offset(from));
        const auto remaining = size() - (to - from);
        while (size() > remaining) pop_back();
      }
    return offset(from);
  }
  constexpr void swap(inplace_vector& other) noexcept {
    if (this == &other) return;
    if constexpr (N > 0) {
      const auto common = std::min(size(), other.size());
      for (size_type i = 0; i < common; ++i) {
        using std::swap;
        swap((*this)[i], other[i]);
      }
      if (size() > other.size()) {
        const auto finish = size();
        for (size_type i = common; i < finish; ++i) other.unchecked_emplace_back(std::move((*this)[i]));
        while (size() > common) pop_back();
      } else {
        const auto finish = other.size();
        for (size_type i = common; i < finish; ++i) unchecked_emplace_back(std::move(other[i]));
        while (other.size() > common) other.pop_back();
      }
    }
  }
  friend constexpr void swap(inplace_vector& left, inplace_vector& right) noexcept { left.swap(right); }
  [[nodiscard]] friend constexpr bool operator==(const inplace_vector& left, const inplace_vector& right) noexcept
    requires(N == 0 || std::equality_comparable<T>)
  {
    if constexpr (N == 0)
      return true;
    else
      return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
  }
  [[nodiscard]] friend constexpr auto operator<=>(const inplace_vector& left, const inplace_vector& right) noexcept
    requires(N == 0 ||
             requires(const T& a, const T& b) {
               { a < b } -> std::convertible_to<bool>;
             })
  {
    if constexpr (N == 0)
      return std::strong_ordering::equal;
    else
      return std::lexicographical_compare_three_way(
          left.begin(), left.end(), right.begin(), right.end(), [](const T& a, const T& b) {
            if constexpr (std::three_way_comparable<T>)
              return a <=> b;
            else
              return a < b ? std::weak_ordering::less : b < a ? std::weak_ordering::greater : std::weak_ordering::equivalent;
          });
  }

 private:
  [[nodiscard]] constexpr iterator offset(size_type index) noexcept {
    if constexpr (N == 0)
      return nullptr;
    else
      return data() + index;
  }
  [[nodiscard]] constexpr const_iterator offset(size_type index) const noexcept {
    if constexpr (N == 0)
      return nullptr;
    else
      return data() + index;
  }
  [[nodiscard]] constexpr size_type index_of(const_iterator position) const noexcept {
    if constexpr (N == 0) {
      if (position != nullptr) std::abort();
      return 0;
    } else {
      if (position < begin() || position > end()) std::abort();
      return static_cast<size_type>(position - begin());
    }
  }
};

template <class T, std::size_t N, class U = T>
constexpr typename inplace_vector<T, N>::size_type erase(inplace_vector<T, N>& values, const U& value) noexcept {
  const auto old_size = values.size();
  if constexpr (N > 0) values.erase(std::remove(values.begin(), values.end(), value), values.end());
  return old_size - values.size();
}
template <class T, std::size_t N, class Predicate>
constexpr typename inplace_vector<T, N>::size_type erase_if(inplace_vector<T, N>& values, Predicate predicate) noexcept {
  const auto old_size = values.size();
  if constexpr (N > 0) values.erase(std::remove_if(values.begin(), values.end(), predicate), values.end());
  return old_size - values.size();
}

namespace freestanding {
/** @brief Explicit opt-in subset that forbids unbounded checked construction and insertion. */
template <class T, std::size_t N>
class inplace_vector : public container::inplace_vector<T, N> {
  using Base = container::inplace_vector<T, N>;

 public:
  inplace_vector() noexcept = default;
  inplace_vector(const inplace_vector&) noexcept = default;
  inplace_vector(inplace_vector&&) noexcept = default;
  inplace_vector& operator=(const inplace_vector&) noexcept = default;
  inplace_vector& operator=(inplace_vector&&) noexcept = default;
  template <class... Args>
  inplace_vector(detail::freestanding_deleted, Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
    requires(!(sizeof...(Args) == 1 && (std::is_same_v<std::remove_cvref_t<Args>, inplace_vector> && ...)))
  inplace_vector(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  inplace_vector& operator=(std::initializer_list<T>) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void assign(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class R>
  void assign_range(R&&) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  void at(typename Base::size_type) const =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void resize(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  void reserve(typename Base::size_type) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void emplace_back(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void push_back(Args&&...) =
      delete ("unbounded operation is banned in freestanding mode; use try_push_back or unchecked_push_back");
  template <class R>
  void append_range(R&&) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void emplace(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void insert(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
  template <class... Args>
  void insert_range(Args&&...) =
      delete ("unbounded inplace_vector operation is banned in freestanding mode; use try_* or unchecked_*");
};
}  // namespace freestanding
}  // namespace uarch::container
