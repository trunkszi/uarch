#pragma once
#include <array>
#include <cstdlib>
#include <limits>
#include <mdspan>
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/no_padding.hpp>
#include <utility>
namespace uarch::layout {
namespace detail {
template <class Record, std::meta::info M>
concept SoaMember = std::meta::is_nonstatic_data_member(M) && std::meta::parent_of(M) == ^^Record;
// Each column is a distinct array object; flattening the tuple with pointer arithmetic would be undefined.
template <class T, std::size_t N, std::size_t K>
struct ColumnAccessor {
  using offset_policy = ColumnAccessor;
  using element_type = T;
  using reference = T&;
  struct data_handle_type {
    std::array<T*, K> columns{};
    std::size_t offset{0};
  };
  constexpr reference access(data_handle_type p, std::size_t i) const noexcept {
    const auto linear = p.offset + i;
    return p.columns[linear / N][linear % N];
  }
  constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept {
    p.offset += i;
    return p;
  }
};
template <class T>
constexpr void copy_member(T& dest, const T& src) noexcept {
  if constexpr (std::is_array_v<T>) {
    for (std::size_t i = 0; i < std::extent_v<T>; ++i) copy_member(dest[i], src[i]);
  } else
    dest = src;
}
}  // namespace detail
/// Owning, fixed-capacity columns. Spans and matrices borrow this object's storage.
template <class Record, std::size_t N>
class SoaColumns {
  static_assert(std::is_class_v<Record> && NoPadding<Record>, std::string_view("SoaColumns requires a padding-free record"));
  static_assert(N > 0, std::string_view("SoaColumns capacity must be positive"));
  static constexpr auto members =
      std::define_static_array(std::meta::nonstatic_data_members_of(^^Record, reflect::unchecked_context()));
  static_assert(!members.empty(), std::string_view("SoaColumns requires at least one member"));
  static_assert(N <= std::numeric_limits<std::size_t>::max() / members.size());

 public:
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  using MemberType = [:std::meta::type_of(M):];

 private:
  template <std::size_t... I>
  static auto storage_type(std::index_sequence<I...>) -> std::tuple<CacheAligned<std::array<MemberType<members[I]>, N>>...>;
  using Storage = decltype(storage_type(std::make_index_sequence<members.size()>{}));
  Storage columns_{};
  template <std::meta::info M>
  static consteval std::size_t index_of() {
    for (std::size_t i = 0; i < members.size(); ++i)
      if (members[i] == M) return i;
    std::abort();
  }
  using First = MemberType<members[0]>;
  static consteval bool homogeneous() {
    bool same = true;
    template for (constexpr auto m : members) { same = same && std::same_as<First, MemberType<m>>; }
    return same && !std::is_array_v<First>;
  }
  template <class Self>
  static constexpr auto matrix_for(Self& self) noexcept {
    using T = std::conditional_t<std::is_const_v<Self>, const First, First>;
    using Accessor = detail::ColumnAccessor<T, N, members.size()>;
    using Extents = std::extents<std::size_t, N, members.size()>;
    typename Accessor::data_handle_type handle{};
    template for (constexpr auto m : members) { handle.columns[index_of<m>()] = self.template column_data<m>(); }
    return std::mdspan<T, Extents, std::layout_left, Accessor>(handle, typename std::layout_left::mapping<Extents>{});
  }

 public:
  static constexpr std::size_t size() noexcept { return N; }
  static constexpr std::size_t column_count() noexcept { return members.size(); }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  constexpr auto& column_aligned() noexcept {
    return std::get<index_of<M>()>(columns_);
  }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  constexpr const auto& column_aligned() const noexcept {
    return std::get<index_of<M>()>(columns_);
  }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  constexpr std::span<MemberType<M>, N> column() noexcept {
    return column_aligned<M>().value;
  }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  constexpr std::span<const MemberType<M>, N> column() const noexcept {
    return column_aligned<M>().value;
  }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  [[nodiscard]] constexpr MemberType<M>* column_data() noexcept {
    if consteval {
      return column<M>().data();
    } else {
      return std::assume_aligned<64>(column<M>().data());
    }
  }
  template <std::meta::info M>
    requires detail::SoaMember<Record, M>
  [[nodiscard]] constexpr const MemberType<M>* column_data() const noexcept {
    if consteval {
      return column<M>().data();
    } else {
      return std::assume_aligned<64>(column<M>().data());
    }
  }
  constexpr Record row(std::size_t index) const noexcept {
    if (index >= N) std::abort();
    Record r{};
    template for (constexpr auto m : members) { detail::copy_member(r.[:m:], column<m>()[index]); }
    return r;
  }
  constexpr void set_row(std::size_t index, const Record& r) noexcept {
    if (index >= N) std::abort();
    template for (constexpr auto m : members) { detail::copy_member(column<m>()[index], r.[:m:]); }
  }
  constexpr auto matrix() noexcept
    requires(homogeneous())
  {
    return matrix_for(*this);
  }
  constexpr auto matrix() const noexcept
    requires(homogeneous())
  {
    return matrix_for(*this);
  }
};
}  // namespace uarch::layout
