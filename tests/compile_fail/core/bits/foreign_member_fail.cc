// expected-error: no matching
#include <uarch/core/bits/bit_word.hpp>
struct Own {
  using Word = std::uint64_t;
  [[= uarch::bits::Field{0, 8}]] std::uint8_t value;
};
struct Foreign {
  using Word = std::uint64_t;
  [[= uarch::bits::Field{0, 8}]] std::uint8_t value;
};
auto rejected = uarch::bits::BitWord<Own>{}.get<^^Foreign::value>();
