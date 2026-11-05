/** @file @brief Fixed-width GNU-vector SIMD with one representation in constant and runtime evaluation. */
#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace uarch::simd {
/** @brief Exactly fixed-width integer lanes and IEEE-sized float/double lanes. */
template <class T>
concept SimdElement =
    std::same_as<T, std::int8_t> || std::same_as<T, std::uint8_t> || std::same_as<T, std::int16_t> ||
    std::same_as<T, std::uint16_t> || std::same_as<T, std::int32_t> || std::same_as<T, std::uint32_t> ||
    std::same_as<T, std::int64_t> || std::same_as<T, std::uint64_t> || std::same_as<T, float> || std::same_as<T, double>;
namespace detail {
template <class T, int N>
concept ValidLanes = SimdElement<T> && N > 0 && std::has_single_bit(static_cast<unsigned>(N)) &&
                     sizeof(T) * static_cast<std::size_t>(N) >= 16 && sizeof(T) * static_cast<std::size_t>(N) <= 64;
/** @brief A nested typedef preserves dependent vector_size on GCC as well as Clang. */
template <SimdElement T, int N>
  requires ValidLanes<T, N>
struct storage {
  using signed_int_t = std::conditional_t<
      sizeof(T) == 1, std::int8_t,
      std::conditional_t<sizeof(T) == 2, std::int16_t, std::conditional_t<sizeof(T) == 4, std::int32_t, std::int64_t>>>;
  typedef T value_vector_type __attribute__((vector_size(sizeof(T) * static_cast<std::size_t>(N))));
  typedef signed_int_t mask_vector_type __attribute__((vector_size(sizeof(T) * static_cast<std::size_t>(N))));
};
template <SimdElement T>
constexpr T Add(T a, T b) noexcept {
  if constexpr (std::integral<T>) {
    using U = std::make_unsigned_t<T>;
    return std::bit_cast<T>(static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(a)) + static_cast<U>(b)));
  } else
    return a + b;
}
template <SimdElement T>
struct AddOp {
  static constexpr T operator()(T a, T b) noexcept { return Add(a, b); }
};
}  // namespace detail

template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
class vec;
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
class mask;
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr vec<T, N> select(mask<T, N> m, vec<T, N> yes, vec<T, N> no) noexcept;

/** @brief Canonical all-one/zero signed mask lanes, with boolean accessors.
 * Wrappers wider than 16 bytes have target-dependent vector calling conventions (-Wpsabi).
 * Across differing raw TU/target ISA flags, use arch::run/dispatch_kernel or references/spans.
 */
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
class mask {
  using S = detail::storage<T, N>;
  typename S::mask_vector_type raw_{};
  friend class vec<T, N>;
  friend constexpr vec<T, N> select<T, N>(mask, vec<T, N>, vec<T, N>) noexcept;

 public:
  using lane_type = typename S::signed_int_t;
  static constexpr int size() noexcept { return N; }
  constexpr mask() noexcept = default;
  [[gnu::always_inline]] constexpr explicit mask(bool value) noexcept {
    std::array<lane_type, static_cast<std::size_t>(N)> lanes{};
    for (auto& lane : lanes) lane = value ? static_cast<lane_type>(-1) : 0;
    if consteval {
      raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      __builtin_memcpy(&raw_, lanes.data(), sizeof(raw_));
    }
  }
  [[gnu::always_inline]] constexpr bool operator[](int i) const noexcept {
    if (i < 0 || i >= N) std::abort();
    if consteval {
      return std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(raw_)[static_cast<std::size_t>(i)] != 0;
    } else {
      return raw_[i] != 0;
    }
  }
  [[gnu::always_inline]] friend constexpr mask operator&(const mask& a, const mask& b) noexcept {
    mask result;
    if consteval {
      auto lanes = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(a.raw_);
      auto other = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(b.raw_);
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] =
            static_cast<lane_type>(lanes[static_cast<std::size_t>(i)] & other[static_cast<std::size_t>(i)]);
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = a.raw_ & b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr mask operator|(const mask& a, const mask& b) noexcept {
    mask result;
    if consteval {
      auto lanes = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(a.raw_);
      auto other = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(b.raw_);
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] =
            static_cast<lane_type>(lanes[static_cast<std::size_t>(i)] | other[static_cast<std::size_t>(i)]);
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = a.raw_ | b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr mask operator^(const mask& a, const mask& b) noexcept {
    mask result;
    if consteval {
      auto lanes = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(a.raw_);
      auto other = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(b.raw_);
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] =
            static_cast<lane_type>(lanes[static_cast<std::size_t>(i)] ^ other[static_cast<std::size_t>(i)]);
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = a.raw_ ^ b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr mask operator~(const mask& input) noexcept {
    mask result;
    if consteval {
      auto lanes = std::bit_cast<std::array<lane_type, static_cast<std::size_t>(N)>>(input.raw_);
      for (auto& lane : lanes) lane = static_cast<lane_type>(~lane);
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = ~input.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr mask operator!(const mask& input) noexcept { return ~input; }
};

/** @brief Fixed lane count independent of translation-unit ISA flags.
 * Wrappers wider than 16 bytes have target-dependent vector calling conventions (-Wpsabi).
 * Raw by-value cross-TU calls compiled with different -m flags are not ABI-compatible; cross target
 * boundaries through arch::run/dispatch_kernel or references/spans.
 */
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
class vec {
  using S = detail::storage<T, N>;
  typename S::value_vector_type raw_{};
  friend constexpr vec select<T, N>(mask<T, N>, vec, vec) noexcept;

 public:
  using value_type = T;
  using mask_type = mask<T, N>;
  static constexpr int size() noexcept { return N; }
  constexpr vec() noexcept = default;
  [[gnu::always_inline]] constexpr explicit vec(T value) noexcept {
    std::array<T, static_cast<std::size_t>(N)> lanes{};
    for (auto& lane : lanes) lane = value;
    *this = from_array(lanes);
  }
  [[nodiscard, gnu::always_inline]] static constexpr vec from_array(
      const std::array<T, static_cast<std::size_t>(N)>& lanes) noexcept {
    vec result;
    if consteval {
      result.raw_ = std::bit_cast<typename S::value_vector_type>(lanes);
    } else {
      __builtin_memcpy(&result.raw_, lanes.data(), sizeof(result.raw_));
    }
    return result;
  }
  [[nodiscard, gnu::always_inline]] constexpr std::array<T, static_cast<std::size_t>(N)> to_array() const noexcept {
    if consteval {
      return std::bit_cast<std::array<T, static_cast<std::size_t>(N)>>(raw_);
    } else {
      std::array<T, static_cast<std::size_t>(N)> result{};
      __builtin_memcpy(result.data(), &raw_, sizeof(raw_));
      return result;
    }
  }
  [[nodiscard, gnu::always_inline]] constexpr T operator[](int i) const noexcept {
    if (i < 0 || i >= N) std::abort();
    if consteval {
      return to_array()[static_cast<std::size_t>(i)];
    } else {
      return raw_[i];
    }
  }
  [[gnu::always_inline]] friend constexpr vec operator+(const vec& a, const vec& b) noexcept {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) + static_cast<U>(other[index])));
        } else {
          lanes[index] = lanes[index] + other[index];
        }
      }
      result = from_array(lanes);
    } else {
      if constexpr (std::integral<T>) {
        using U = std::make_unsigned_t<T>;
        using UV = typename detail::storage<U, N>::value_vector_type;
        UV left, right, output;
        __builtin_memcpy(&left, &a.raw_, sizeof(left));
        __builtin_memcpy(&right, &b.raw_, sizeof(right));
        output = left + right;
        __builtin_memcpy(&result.raw_, &output, sizeof(output));
      } else {
        result.raw_ = a.raw_ + b.raw_;
      }
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator+(const vec& a, T b) noexcept { return a + vec{b}; }
  [[gnu::always_inline]] friend constexpr vec operator+(T a, const vec& b) noexcept { return vec{a} + b; }
  [[gnu::always_inline]] friend constexpr vec operator-(const vec& a, const vec& b) noexcept {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) - static_cast<U>(other[index])));
        } else {
          lanes[index] = lanes[index] - other[index];
        }
      }
      result = from_array(lanes);
    } else {
      if constexpr (std::integral<T>) {
        using U = std::make_unsigned_t<T>;
        using UV = typename detail::storage<U, N>::value_vector_type;
        UV left, right, output;
        __builtin_memcpy(&left, &a.raw_, sizeof(left));
        __builtin_memcpy(&right, &b.raw_, sizeof(right));
        output = left - right;
        __builtin_memcpy(&result.raw_, &output, sizeof(output));
      } else {
        result.raw_ = a.raw_ - b.raw_;
      }
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator-(const vec& a, T b) noexcept { return a - vec{b}; }
  [[gnu::always_inline]] friend constexpr vec operator-(T a, const vec& b) noexcept { return vec{a} - b; }
  [[gnu::always_inline]] friend constexpr vec operator*(const vec& a, const vec& b) noexcept {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) * static_cast<U>(other[index])));
        } else {
          lanes[index] = lanes[index] * other[index];
        }
      }
      result = from_array(lanes);
    } else {
      if constexpr (std::integral<T>) {
        using U = std::make_unsigned_t<T>;
        using UV = typename detail::storage<U, N>::value_vector_type;
        UV left, right, output;
        __builtin_memcpy(&left, &a.raw_, sizeof(left));
        __builtin_memcpy(&right, &b.raw_, sizeof(right));
        output = left * right;
        __builtin_memcpy(&result.raw_, &output, sizeof(output));
      } else {
        result.raw_ = a.raw_ * b.raw_;
      }
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator*(const vec& a, T b) noexcept { return a * vec{b}; }
  [[gnu::always_inline]] friend constexpr vec operator*(T a, const vec& b) noexcept { return vec{a} * b; }
  [[gnu::always_inline]] friend constexpr vec operator&(const vec& a, const vec& b) noexcept
    requires std::integral<T>
  {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) & static_cast<U>(other[index])));
        }
      }
      result = from_array(lanes);
    } else {
      result.raw_ = a.raw_ & b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator&(const vec& a, T b) noexcept
    requires std::integral<T>
  {
    return a & vec{b};
  }
  [[gnu::always_inline]] friend constexpr vec operator&(T a, const vec& b) noexcept
    requires std::integral<T>
  {
    return vec{a} & b;
  }
  [[gnu::always_inline]] friend constexpr vec operator|(const vec& a, const vec& b) noexcept
    requires std::integral<T>
  {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) | static_cast<U>(other[index])));
        }
      }
      result = from_array(lanes);
    } else {
      result.raw_ = a.raw_ | b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator|(const vec& a, T b) noexcept
    requires std::integral<T>
  {
    return a | vec{b};
  }
  [[gnu::always_inline]] friend constexpr vec operator|(T a, const vec& b) noexcept
    requires std::integral<T>
  {
    return vec{a} | b;
  }
  [[gnu::always_inline]] friend constexpr vec operator^(const vec& a, const vec& b) noexcept
    requires std::integral<T>
  {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      auto other = b.to_array();
      for (int i = 0; i < N; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if constexpr (std::integral<T>) {
          using U = std::make_unsigned_t<T>;
          lanes[index] = std::bit_cast<T>(
              static_cast<U>(static_cast<std::uint64_t>(static_cast<U>(lanes[index])) ^ static_cast<U>(other[index])));
        }
      }
      result = from_array(lanes);
    } else {
      result.raw_ = a.raw_ ^ b.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] friend constexpr vec operator^(const vec& a, T b) noexcept
    requires std::integral<T>
  {
    return a ^ vec{b};
  }
  [[gnu::always_inline]] friend constexpr vec operator^(T a, const vec& b) noexcept
    requires std::integral<T>
  {
    return vec{a} ^ b;
  }
  [[gnu::always_inline]] friend constexpr vec operator~(const vec& a) noexcept
    requires std::integral<T>
  {
    vec result;
    if consteval {
      auto lanes = a.to_array();
      for (auto& lane : lanes) lane = static_cast<T>(~lane);
      result = from_array(lanes);
    } else {
      result.raw_ = ~a.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator==(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] == right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ == other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator==(T other) const noexcept { return *this == vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator==(T a, const vec& b) noexcept { return vec{a} == b; }
  [[gnu::always_inline]] constexpr mask_type operator!=(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] != right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ != other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator!=(T other) const noexcept { return *this != vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator!=(T a, const vec& b) noexcept { return vec{a} != b; }
  [[gnu::always_inline]] constexpr mask_type operator<(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] < right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ < other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator<(T other) const noexcept { return *this < vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator<(T a, const vec& b) noexcept { return vec{a} < b; }
  [[gnu::always_inline]] constexpr mask_type operator<=(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] <= right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ <= other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator<=(T other) const noexcept { return *this <= vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator<=(T a, const vec& b) noexcept { return vec{a} <= b; }
  [[gnu::always_inline]] constexpr mask_type operator>(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] > right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ > other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator>(T other) const noexcept { return *this > vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator>(T a, const vec& b) noexcept { return vec{a} > b; }
  [[gnu::always_inline]] constexpr mask_type operator>=(const vec& other) const noexcept {
    mask_type result;
    if consteval {
      std::array<typename S::signed_int_t, static_cast<std::size_t>(N)> lanes{};
      auto left = to_array();
      auto right = other.to_array();
      for (int i = 0; i < N; ++i)
        lanes[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)] >= right[static_cast<std::size_t>(i)]
                                                 ? static_cast<typename S::signed_int_t>(-1)
                                                 : 0;
      result.raw_ = std::bit_cast<typename S::mask_vector_type>(lanes);
    } else {
      result.raw_ = raw_ >= other.raw_;
    }
    return result;
  }
  [[gnu::always_inline]] constexpr mask_type operator>=(T other) const noexcept { return *this >= vec{other}; }
  [[gnu::always_inline]] friend constexpr mask_type operator>=(T a, const vec& b) noexcept { return vec{a} >= b; }
};

/** @brief Load N initialized readable elements; no additional pointer alignment requirement. */
template <class V>
[[nodiscard, gnu::always_inline]] constexpr V unchecked_load(const typename V::value_type* pointer) noexcept {
  std::array<typename V::value_type, static_cast<std::size_t>(V::size())> lanes{};
  if consteval {
    for (int i = 0; i < V::size(); ++i) lanes[static_cast<std::size_t>(i)] = pointer[i];
  } else {
    __builtin_memcpy(lanes.data(), pointer, sizeof(lanes));
  }
  return V::from_array(lanes);
}
template <class V>
[[nodiscard, gnu::always_inline]] constexpr V unchecked_load(
    std::span<const typename V::value_type, static_cast<std::size_t>(V::size())> source) noexcept {
  return unchecked_load<V>(source.data());
}
/** @brief Select entire lanes; the mask never aliases the lane representation. */
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr vec<T, N> select(mask<T, N> m, vec<T, N> yes, vec<T, N> no) noexcept {
  vec<T, N> result;
  if consteval {
    auto a = yes.to_array();
    auto b = no.to_array();
    for (int i = 0; i < N; ++i)
      if (!m[i]) a[static_cast<std::size_t>(i)] = b[static_cast<std::size_t>(i)];
    result = vec<T, N>::from_array(a);
  } else {
    result.raw_ = m.raw_ ? yes.raw_ : no.raw_;
  }
  return result;
}
template <SimdElement T, int N, class Op = detail::AddOp<T>>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr T reduce(vec<T, N> input, Op op = {}) noexcept;
namespace detail {
template <SimdElement T, int N, std::size_t... I>
[[nodiscard, gnu::always_inline]] constexpr T FoldHalves(vec<T, N> input, std::index_sequence<I...>) noexcept {
  using U = std::make_unsigned_t<T>;
  using Raw = typename storage<U, N>::value_vector_type;
  Raw raw{};
  __builtin_memcpy(&raw, &input, sizeof(raw));
  const auto low = __builtin_shufflevector(raw, raw, static_cast<int>(I)...);
  const auto high = __builtin_shufflevector(raw, raw, static_cast<int>(I + N / 2)...);
  const auto folded = low + high;
  const auto result = vec<U, N / 2>::from_array(std::bit_cast<std::array<U, static_cast<std::size_t>(N / 2)>>(folded));
  return std::bit_cast<T>(reduce(result));
}
template <int Active, SimdElement T, int N, std::size_t... I>
[[nodiscard, gnu::always_inline]] constexpr T FoldAdd(vec<T, N> input, std::index_sequence<I...>) noexcept {
  if constexpr (Active == 2) {
    return Add(input[0], input[1]);
  } else {
    using Raw = typename storage<T, N>::value_vector_type;
    Raw raw{};
    __builtin_memcpy(&raw, &input, sizeof(raw));
    const auto even = __builtin_shufflevector(raw, raw, static_cast<int>((I % (Active / 2)) * 2)...);
    const auto odd = __builtin_shufflevector(raw, raw, static_cast<int>((I % (Active / 2)) * 2 + 1)...);
    const auto left = vec<T, N>::from_array(std::bit_cast<std::array<T, static_cast<std::size_t>(N)>>(even));
    const auto right = vec<T, N>::from_array(std::bit_cast<std::array<T, static_cast<std::size_t>(N)>>(odd));
    return FoldAdd<Active / 2>(left + right, std::make_index_sequence<static_cast<std::size_t>(N)>{});
  }
}
}  // namespace detail
/** @brief Balanced pairwise reduction; signed integer default addition wraps modulo the lane width. */
template <SimdElement T, int N, class Op>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr T reduce(vec<T, N> input, Op op) noexcept {
  if !consteval {
    if constexpr (std::same_as<Op, detail::AddOp<T>> && std::integral<T> && sizeof(T) * N >= 32)
      return detail::FoldHalves(input, std::make_index_sequence<static_cast<std::size_t>(N / 2)>{});
    else if constexpr (std::same_as<Op, detail::AddOp<T>> && N >= 4)
      return detail::FoldAdd<N>(input, std::make_index_sequence<static_cast<std::size_t>(N)>{});
  }
  auto lanes = input.to_array();
  for (int width = N; width > 1; width /= 2)
    for (int i = 0; i < width / 2; ++i)
      lanes[static_cast<std::size_t>(i)] =
          op(lanes[(static_cast<std::size_t>(i) * 2)], lanes[(static_cast<std::size_t>(i) * 2 + 1)]);
  return lanes[0];
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr T reduce_min(vec<T, N> input) noexcept {
  return reduce(input, [](T a, T b) noexcept { return b < a ? b : a; });
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr T reduce_max(vec<T, N> input) noexcept {
  return reduce(input, [](T a, T b) noexcept { return a < b ? b : a; });
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr std::uint32_t reduce_count(const mask<T, N>& input) noexcept {
  std::uint32_t count = 0;
  for (int i = 0; i < N; ++i) count += input[i] ? 1U : 0U;
  return count;
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr bool all_of(const mask<T, N>& input) noexcept {
  return reduce_count(input) == static_cast<std::uint32_t>(N);
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr bool any_of(const mask<T, N>& input) noexcept {
  return reduce_count(input) != 0;
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr bool none_of(const mask<T, N>& input) noexcept {
  return !any_of(input);
}
template <SimdElement T, int N>
  requires detail::ValidLanes<T, N>
[[nodiscard, gnu::always_inline]] constexpr std::optional<std::uint32_t> find_first_set(const mask<T, N>& input) noexcept {
  std::uint64_t lane_bits = 0;
  for (int i = 0; i < N; ++i) lane_bits |= static_cast<std::uint64_t>(input[i]) << static_cast<unsigned>(i);
  if (lane_bits == 0) return std::nullopt;
  return static_cast<std::uint32_t>(std::countr_zero(lane_bits));
}
/** @brief Token-selected width, with a 16-byte software lane group for scalar tokens. */
template <SimdElement T, class Arch>
using native_vec = vec<T, static_cast<int>((Arch::vector_bytes < 16 ? 16 : Arch::vector_bytes) / sizeof(T))>;
}  // namespace uarch::simd
