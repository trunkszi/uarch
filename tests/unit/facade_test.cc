#include <uarch/uarch.hpp>
struct Sample {
  std::uint64_t price;
  std::uint64_t quantity;
};
struct Header {
  using Word = std::uint64_t;
  bool ready[[= uarch::bits::Flag{0}]];
  std::uint8_t count[[= uarch::bits::Field{1, 7}]];
};
static_assert(uarch::NoPadding<Sample> && uarch::RegisterPassable<Sample>);
static_assert(uarch::Arch<uarch::Scalar>);
int main() {
  using namespace uarch::prelude;
  SoaColumns<Sample, 4> columns;
  columns.set_row(1, Sample{7, 9});
  auto divisor = FastDivisorU64::try_new(3);
  if (!divisor || divisor->div(columns.row(1).quantity) != 3) return 1;
  auto selected = select_register(predicate(true), columns.row(1).price, std::uint64_t{0});
  auto word = BitWord<Header>{};
  auto bitmap = Bitmap64{};
  uarch::container::inplace_vector<int, 4> vector;
  if (vector.try_push_back(7) == nullptr) return 1;
  if (selected != 7 || word.raw() != 0 || bitmap.raw() != 0) return 1;
  uarch::Scalar scalar;
  return scalar.bit_gather(0xA0, 0xF0) != 10;
}
