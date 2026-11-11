// expected-error: overlapping field: collision
#include <uarch/core/bits/bit_word.hpp>
struct Overlap {
  using Word = std::uint64_t;
  [[= uarch::bits::Field{0, 8}]] std::uint8_t first;
  [[= uarch::bits::Field{7, 8}]] std::uint8_t collision;
};
uarch::bits::BitWord<Overlap> rejected;
