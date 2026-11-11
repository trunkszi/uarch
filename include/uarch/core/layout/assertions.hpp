/** @file @brief Exact reflected layout diagnostics and separate geometry/ABI proofs. */
#pragma once
#include <array>
#include <uarch/core/layout/register_passable.hpp>
namespace uarch::layout {
template <class T, std::size_t Size, std::size_t Align = alignof(T), std::size_t... Offsets>
[[nodiscard]] consteval std::string_view layout_report() noexcept {
  using U = std::remove_cv_t<T>;
  if constexpr (sizeof(U) != Size)
    return detail::storage_error<U>("size mismatch: actual " + detail::decimal(sizeof(U)) + "; expected " +
                                    detail::decimal(Size));
  else if constexpr (alignof(U) != Align)
    return detail::storage_error<U>("alignment mismatch: actual " + detail::decimal(alignof(U)) + "; expected " +
                                    detail::decimal(Align));
  else if constexpr (!std::is_class_v<U> || std::is_union_v<U> || !std::is_standard_layout_v<U>)
    return detail::storage_error<U>("standard-layout record required");
  else {
    constexpr auto kContext = reflect::unchecked_context();
    constexpr auto kBases = std::define_static_array(std::meta::bases_of(^^U, kContext));
    if constexpr (!kBases.empty()) return detail::storage_error<U>("base classes are unsupported");
    constexpr auto kMembers = std::define_static_array(std::meta::nonstatic_data_members_of(^^U, kContext));
    constexpr auto kCount = kMembers.size();
    if constexpr (sizeof...(Offsets) > 0 && kCount != sizeof...(Offsets))
      return detail::storage_error<U>("member count mismatch");
    constexpr std::array<std::size_t, sizeof...(Offsets)> kOffsets{Offsets...};
    std::size_t index = 0;
    template for (constexpr auto kMember : std::define_static_array(std::meta::nonstatic_data_members_of(^^U, kContext))) {
      if constexpr (std::meta::is_bit_field(kMember))
        return detail::storage_error<U>("bitfields are unsupported");
      else if constexpr (sizeof...(Offsets) > 0) {
        constexpr auto kActual = reflect::byte_offset(kMember);
        if (kActual != kOffsets[index])
          return detail::member_error<U>(kMember, kActual, "expected byte " + detail::decimal(kOffsets[index]));
        ++index;
      }
    }
    return {};
  }
}
template <class T, std::size_t Size, std::size_t Align = alignof(T), std::size_t... Offsets>
[[nodiscard]] consteval bool check_layout() noexcept {
  return layout_report<T, Size, Align, Offsets...>().empty();
}
template <class T, std::size_t Size, std::size_t Align = alignof(T), std::size_t... Offsets>
consteval bool assert_layout() noexcept {
  assert_no_padding<T>();
  static_assert(check_layout<T, Size, Align, Offsets...>(), layout_report<T, Size, Align, Offsets...>());
  return true;
}
template <class T, std::size_t Lines = 1>
concept CacheLineSized = (Lines > 0) && alignof(T) == 64 && sizeof(T) / 64 == Lines && sizeof(T) % 64 == 0;
template <class T, std::size_t Lines = 1>
concept CacheLineDense = CacheLineSized<T, Lines> && NoPadding<T>;
template <class T, std::size_t Pairs = 1>
concept IsolatedSized = (Pairs > 0) && alignof(T) == 128 && sizeof(T) / 128 == Pairs && sizeof(T) % 128 == 0;
template <class T, std::size_t Lines = 1>
consteval bool assert_cache_line() noexcept {
  static_assert(CacheLineSized<T, Lines>);
  return true;
}
template <class T, std::size_t Pairs = 1>
consteval bool assert_isolated() noexcept {
  static_assert(IsolatedSized<T, Pairs>);
  return true;
}
template <class T>
consteval bool assert_register_pass() noexcept {
  static_assert(RegisterPassable<T>);
  return true;
}
template <class T>
consteval bool assert_gpr_pair() noexcept {
  static_assert(GprPair<T>);
  return true;
}
}  // namespace uarch::layout
