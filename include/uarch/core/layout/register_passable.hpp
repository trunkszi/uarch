/** @file @brief Scalar-leaf annotations and aggregate register eligibility proofs. */
#pragma once
#include <uarch/core/layout/abi_class.hpp>
#include <uarch/core/layout/no_padding.hpp>

namespace uarch::layout {
struct register_leaf_tag {};
inline constexpr register_leaf_tag register_leaf{};

namespace detail {
template <class T>
consteval bool register_leaf_proof() {
  using U = std::remove_cv_t<T>;
  if constexpr (!NoPadding<U>) return false;
  if constexpr (std::is_integral_v<U> || std::is_enum_v<U> || std::is_pointer_v<U> || std::is_same_v<U, float> ||
                std::is_same_v<U, double>)
    return sizeof(U) <= 8;
  else if constexpr (std::is_class_v<U>) {
    // Annotations belong to the underlying class, not the local type alias.
    if constexpr (reflect::annotation_count_of<register_leaf_tag>(std::meta::dealias(^^U)) != 1)
      return false;
    else {
      std::size_t nonempty = 0;
      bool valid = true;
      template for (constexpr auto member :
                    std::define_static_array(std::meta::nonstatic_data_members_of(^^U, reflect::unchecked_context()))) {
        if constexpr (std::meta::is_bit_field(member))
          valid = false;
        else {
          using M = [:std::meta::type_of(member):];
          if constexpr (!std::is_empty_v<M>) {
            ++nonempty;
            valid = valid && reflect::byte_offset(member) == 0 && sizeof(M) == sizeof(U) && register_leaf_proof<M>();
          }
        }
      }
      return valid && nonempty == 1;
    }
  } else
    return false;
}
}  // namespace detail

template <class T>
concept RegisterLeaf = detail::register_leaf_proof<T>();
template <class T>
concept CallTrivial = detail::call_trivial<T>;
/// Empty sealed permissions have no SysV argument registers; they are not payload storage.
template <class T>
concept ZeroRegisterCapability =
    std::is_empty_v<T> && !std::is_trivially_copyable_v<T> && CallTrivial<T> && sysv_classify<T>().registers == 0;
template <class T>
concept RegisterPassable = CallTrivial<T> && NoPadding<T> && sysv_classify<T>().lo != AbiClass::Memory;
template <class T>
concept GprPair = RegisterPassable<T> && sysv_classify<T>().lo == AbiClass::Integer &&
                  (sizeof(T) <= 8 || sysv_classify<T>().hi == AbiClass::Integer);
}  // namespace uarch::layout
