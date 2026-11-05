/** @file @brief Conservative native aggregate ABI classification before codegen auditing. */
#pragma once
#include <cstdint>
#include <type_traits>
#include <uarch/core/reflect.hpp>

namespace uarch::layout {
enum class AbiClass : std::uint8_t { None, Integer, Sse, Memory };
struct AbiShape {
  AbiClass lo{AbiClass::None};
  AbiClass hi{AbiClass::None};
  std::uint8_t registers{0};
  constexpr bool operator==(const AbiShape&) const = default;
};

namespace detail {
constexpr AbiShape memory_shape() noexcept { return {AbiClass::Memory, AbiClass::Memory, 0}; }
// Assignment does not participate in the ABI trivial-for-calls rule.
template <class T>
inline constexpr bool call_trivial = std::is_trivially_destructible_v<T> && std::is_trivially_copy_constructible_v<T> &&
                                     std::is_trivially_move_constructible_v<T>;
template <class T>
consteval bool abi_trivial() {
  if constexpr (std::is_bounded_array_v<T>)
    return abi_trivial<std::remove_extent_t<T>>();
  else
    return call_trivial<T>;
}
constexpr AbiClass merge(AbiClass a, AbiClass b) noexcept {
  if (a == AbiClass::Memory || b == AbiClass::Memory) return AbiClass::Memory;
  if (a == AbiClass::None) return b;
  if (b == AbiClass::None) return a;
  return a == AbiClass::Integer || b == AbiClass::Integer ? AbiClass::Integer : AbiClass::Sse;
}
template <class T>
consteval bool visit_eightbytes(AbiShape& shape, std::size_t offset) {
  using U = std::remove_cv_t<T>;
  if constexpr (!abi_trivial<U>() || std::is_union_v<U> || std::is_same_v<U, long double>) {
    return false;
  }
  if constexpr (std::is_integral_v<U> || std::is_pointer_v<U> || std::is_enum_v<U> || std::is_same_v<U, __int128_t> ||
                std::is_same_v<U, __uint128_t> || std::is_same_v<U, float> || std::is_same_v<U, double>) {
    if (offset % alignof(U) != 0 || offset + sizeof(U) > 16) return false;
    constexpr auto leaf = std::is_same_v<U, float> || std::is_same_v<U, double> ? AbiClass::Sse : AbiClass::Integer;
    if constexpr (sizeof(U) > 8) {
      if constexpr (!std::is_same_v<U, __int128_t> && !std::is_same_v<U, __uint128_t>) return false;
      if (offset != 0) return false;
      shape.lo = merge(shape.lo, leaf);
      shape.hi = merge(shape.hi, leaf);
    } else {
      if (offset / 8 != (offset + sizeof(U) - 1) / 8) return false;
      auto& lane = offset < 8 ? shape.lo : shape.hi;
      lane = merge(lane, leaf);
    }
    return true;
  } else if constexpr (std::is_bounded_array_v<U>) {
    using M = std::remove_extent_t<U>;
    for (std::size_t i = 0; i < std::extent_v<U>; ++i)
      if (!visit_eightbytes<M>(shape, offset + i * sizeof(M))) return false;
    return true;
  } else if constexpr (std::is_class_v<U> && std::is_standard_layout_v<U>) {
    constexpr auto ctx = reflect::unchecked_context();
    constexpr auto bases = std::define_static_array(std::meta::bases_of(^^U, ctx));
    if constexpr (!bases.empty())
      return false;
    else {
      template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^U, ctx))) {
        if constexpr (std::meta::is_bit_field(member))
          return false;
        else {
          using M = [:std::meta::type_of(member):];
          if constexpr (!std::is_empty_v<M>) {
            if (!visit_eightbytes<M>(shape, offset + reflect::byte_offset(member))) return false;
          }
        }
      }
      return true;
    }
  } else
    return false;
}

struct HomogeneousFloats {
  std::size_t width{};
  std::uint8_t count{};
};
template <class T>
consteval bool homogeneous_floats(HomogeneousFloats& result) {
  using U = std::remove_cv_t<T>;
  if constexpr (std::is_same_v<U, float> || std::is_same_v<U, double>) {
    if (result.count == 4 || (result.width != 0 && result.width != sizeof(U))) return false;
    result.width = sizeof(U);
    ++result.count;
    return true;
  } else if constexpr (std::is_bounded_array_v<U>) {
    for (std::size_t i = 0; i < std::extent_v<U>; ++i)
      if (!homogeneous_floats<std::remove_extent_t<U>>(result)) return false;
    return true;
  } else if constexpr (std::is_class_v<U>) {
    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^U, reflect::unchecked_context()))) {
      if constexpr (std::meta::is_bit_field(member))
        return false;
      else {
        using M = [:std::meta::type_of(member):];
        if constexpr (!std::is_empty_v<M>) {
          if (!homogeneous_floats<M>(result)) return false;
        }
      }
    }
    return true;
  } else
    return false;
}
}  // namespace detail

/// Predicts at most two SysV eightbytes; register exhaustion requires signature-level auditing.
template <class T>
[[nodiscard]] consteval AbiShape sysv_classify() {
  using U = std::remove_cv_t<T>;
  if constexpr (!requires { sizeof(U); })
    return detail::memory_shape();
  else {
    if constexpr (!detail::abi_trivial<U>() || sizeof(U) == 0 || sizeof(U) > 16 || alignof(U) > 16)
      return detail::memory_shape();
    AbiShape shape;
    if (!detail::visit_eightbytes<U>(shape, 0)) return detail::memory_shape();
    shape.registers = static_cast<std::uint8_t>((shape.lo != AbiClass::None ? 1 : 0) + (shape.hi != AbiClass::None ? 1 : 0));
    return shape;
  }
}

/// The supported <=16-byte subset uses up to four HFA registers or up to two GPRs.
template <class T>
[[nodiscard]] consteval AbiShape aapcs64_classify() {
  using U = std::remove_cv_t<T>;
  constexpr auto checked = sysv_classify<U>();
  if constexpr (checked.lo == AbiClass::Memory)
    return detail::memory_shape();
  else {
    detail::HomogeneousFloats floats;
    if (detail::homogeneous_floats<U>(floats) && floats.count != 0 &&
        static_cast<std::size_t>(floats.count) * floats.width == sizeof(U))
      return {AbiClass::Sse, floats.count > 1 ? AbiClass::Sse : AbiClass::None, floats.count};
    if (checked.registers == 0) return {};
    return {AbiClass::Integer, sizeof(U) > 8 ? AbiClass::Integer : AbiClass::None,
            static_cast<std::uint8_t>((sizeof(U) + 7) / 8)};
  }
}
}  // namespace uarch::layout
