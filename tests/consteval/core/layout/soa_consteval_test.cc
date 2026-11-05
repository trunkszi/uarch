#include <uarch/core/layout/soa_columns.hpp>
struct Record {
  std::uint64_t price;
  std::uint64_t size;
};
constexpr bool check() {
  uarch::layout::SoaColumns<Record, 3> columns;
  columns.set_row(1, Record{7, 9});
  auto matrix = columns.matrix();
  matrix[2, 0] = 11;
  return columns.row(1).price == 7 && columns.column<^^Record::size>()[1] == 9 && columns.row(2).price == 11;
}
static_assert(check());
int main() {}
