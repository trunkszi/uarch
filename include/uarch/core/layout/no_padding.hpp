/** @file @brief Reflection proofs for contiguous initialized payload storage. */
#pragma once
#include <climits>
#include <limits>
#include <string>
#include <type_traits>
#include <uarch/core/reflect.hpp>

namespace uarch::layout {
template <class T>
[[nodiscard]] consteval std::string_view padding_report();

namespace detail {
consteval std::string decimal(std::size_t value) {
  std::string digits;
  do {
    digits.insert(digits.begin(), static_cast<char>('0' + value % 10));
    value /= 10;
  } while (value != 0);
  return digits;
}
// Indexed copying also works with reflection-promoted strings under GCC UBSan:
// library view-to-string constructors compare their promoted address with null.
consteval void append_text(std::string& output, std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) output.push_back(text[i++]);
}
template <class T>
consteval std::string_view type_label() {
  constexpr auto type = std::meta::dealias(^^T);
  if constexpr (std::meta::has_identifier(type))
    return std::meta::identifier_of(type);
  else
    return "<type>";
}
template <class T>
consteval std::string_view storage_error(std::string_view reason) {
  std::string result;
  append_text(result, type_label<T>());
  append_text(result, ": ");
  append_text(result, reason);
  return reflect::freeze_string(result);
}
template <class T>
consteval std::string_view member_error(std::meta::info member, std::size_t offset, std::string_view reason) {
  std::string result;
  append_text(result, type_label<T>());
  append_text(result, " member '");
  append_text(result, std::meta::identifier_of(member));
  append_text(result, "' at byte ");
  append_text(result, decimal(offset));
  append_text(result, ": ");
  append_text(result, reason);
  return reflect::freeze_string(result);
}
}  // namespace detail

/// Empty means padding-free under this library's scalar and aggregate rules.
/// This proof grants neither arbitrary-bit validity nor aliasing permissions.
template <class T>
[[nodiscard]] consteval std::string_view padding_report() {
  using U = std::remove_cv_t<T>;
  if constexpr (requires { sizeof(U); }) {
    if constexpr (std::is_same_v<U, bool> || std::is_same_v<U, long double>) {
      return detail::storage_error<U>("unsupported scalar representation");
    }
    if constexpr (std::is_integral_v<U> || std::is_same_v<U, __int128_t> || std::is_same_v<U, __uint128_t>) {
      if constexpr (std::numeric_limits<U>::digits + (std::is_signed_v<U> ? 1 : 0) == sizeof(U) * CHAR_BIT)
        return {};
      else
        return detail::storage_error<U>("integer storage has padding");
    }
    if constexpr (std::is_same_v<U, float> || std::is_same_v<U, double> || std::is_pointer_v<U>) {
      return {};
    }
    if constexpr (std::is_enum_v<U>) {
      return padding_report<std::underlying_type_t<U>>();
    }
    if constexpr (std::is_bounded_array_v<U>) {
      if constexpr (std::extent_v<U> == 0)
        return detail::storage_error<U>("empty array");
      else
        return padding_report<std::remove_extent_t<U>>();
    }
    if constexpr (std::is_class_v<U> && std::is_standard_layout_v<U> &&
                  (std::is_trivially_copyable_v<U> ||
                   (std::is_trivially_copy_constructible_v<U> && std::is_trivially_move_constructible_v<U> &&
                    std::is_trivially_destructible_v<U>))) {
      constexpr auto ctx = reflect::unchecked_context();
      constexpr auto bases = std::define_static_array(std::meta::bases_of(^^U, ctx));
      if constexpr (!bases.empty()) {
        return detail::storage_error<U>("base classes are unsupported");
      } else {
        std::size_t expected = 0;
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^U, ctx))) {
          if constexpr (std::meta::is_bit_field(member)) {
            return detail::member_error<U>(member, static_cast<std::size_t>(std::meta::offset_of(member).bytes), "bitfield");
          } else {
            using M = [:std::meta::type_of(member):];
            constexpr auto offset = reflect::byte_offset(member);
            if constexpr (std::is_empty_v<M>) {
              // Shared empty storage contributes no bytes; an ordinary empty member
              // exposes its consumed byte at the next offset or the final tail check.
              if (offset > expected) return detail::member_error<U>(member, offset, "gap before empty member");
            } else {
              if (offset != expected) {
                return detail::member_error<U>(member, offset, "expected byte " + detail::decimal(expected));
              }
              constexpr auto nested = padding_report<M>();
              if constexpr (!nested.empty()) return detail::member_error<U>(member, offset, nested);
              expected += sizeof(M);
            }
          }
        }
        if (expected != sizeof(U)) {
          return detail::storage_error<U>("tail padding after byte " + detail::decimal(expected) + "; storage size " +
                                          detail::decimal(sizeof(U)));
        }
        return {};
      }
    }
    return detail::storage_error<U>("unsupported aggregate or union");
  } else {
    return detail::storage_error<U>("incomplete or non-object storage");
  }
}

template <class T>
concept NoPadding = padding_report<T>().empty();

/// Emits the offending type/member and byte position at the caller's proof site.
template <class T>
consteval void assert_no_padding() {
  static_assert(NoPadding<T>, padding_report<T>());
}
}  // namespace uarch::layout
