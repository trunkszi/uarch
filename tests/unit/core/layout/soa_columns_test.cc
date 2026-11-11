#include <array>
#include <bit>
#include <csignal>
#include <type_traits>
#include <uarch/core/layout/soa_columns.hpp>
struct Mixed {
  std::uint32_t timestamp;
  std::uint32_t count;
  double weight;
};
struct Equal {
  std::uint64_t timestamp;
  std::uint64_t count;
};
struct ArrayRecord {
  std::uint32_t lanes[2];
};
using uarch::layout::SoaColumns;
template <class T>
concept HasMatrix = requires(T& t) { t.matrix(); };
static_assert(!HasMatrix<SoaColumns<Mixed, 3>>);
static_assert(HasMatrix<SoaColumns<Equal, 3>>);
static_assert(!HasMatrix<SoaColumns<ArrayRecord, 2>>);
static_assert(std::same_as<decltype(std::declval<const SoaColumns<Equal, 3>&>().column<^^Equal::timestamp>()),
                           std::span<const std::uint64_t, 3>>);
static_assert(std::same_as<decltype(std::declval<SoaColumns<Equal, 3>&>().column_aligned<^^Equal::timestamp>()),
                           uarch::layout::CacheAligned<std::array<std::uint64_t, 3>>&>);
int main() {
  SoaColumns<Mixed, 17> columns;
  for (std::size_t i = 0; i < 17; ++i) {
    columns.set_row(i, Mixed{static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(i * 3), static_cast<double>(i)});
  }
  for (std::size_t i = 0; i < 17; ++i) {
    const auto r = columns.row(i);
    if (r.timestamp != i || r.count != i * 3 || r.weight != static_cast<double>(i)) return 1;
  }
  if (reinterpret_cast<std::uintptr_t>(columns.column_data<^^Mixed::timestamp>()) % 64 != 0 ||
      reinterpret_cast<std::uintptr_t>(columns.column_data<^^Mixed::count>()) % 64 != 0 ||
      reinterpret_cast<std::uintptr_t>(columns.column_data<^^Mixed::weight>()) % 64 != 0)
    return 1;
  const auto& const_columns = columns;
  if (const_columns.column_data<^^Mixed::count>() != columns.column<^^Mixed::count>().data()) return 1;
  SoaColumns<Equal, 3> same;
  auto matrix = same.matrix();
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t col = 0; col < 2; ++col) matrix[row, col] = row * 10 + col;
  const auto& const_same = same;
  const auto read = const_same.matrix();
  if (read[2, 1] != 21 || &matrix[0, 0] != same.column_data<^^Equal::timestamp>() ||
      &matrix[0, 1] != same.column_data<^^Equal::count>())
    return 1;
  static_assert(std::same_as<decltype(read[0, 0]), const std::uint64_t&>);
  auto handle = matrix.accessor().offset(matrix.data_handle(), 4);
  if (&matrix.accessor().access(handle, 1) != &matrix[2, 1]) return 1;
  auto copy = same;
  copy.column<^^Equal::timestamp>()[0] = 99;
  if (same.row(0).timestamp != 0 || copy.row(0).timestamp != 99) return 1;
  SoaColumns<ArrayRecord, 2> arrays;
  arrays.set_row(1, ArrayRecord{{7, 9}});
  if (arrays.row(1).lanes[1] != 9) return 1;

  if (std::signal(SIGABRT, +[](int) { std::_Exit(0); }) == SIG_ERR) return 1;
  if constexpr (UARCH_SOA_SET_BOUNDS)
    columns.set_row(17, Mixed{});
  else
    (void)columns.row(17);
  return 1;
}
