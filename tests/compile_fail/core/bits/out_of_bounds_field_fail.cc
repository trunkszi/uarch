// expected-error: outside word bounds: outside
#include <uarch/core/bits/bit_word.hpp>
struct Outside {
  using Word = std::uint8_t;
  [[= uarch::bits::Field{7, 2}]] std::uint8_t outside;
};
uarch::bits::BitWord<Outside> rejected;
