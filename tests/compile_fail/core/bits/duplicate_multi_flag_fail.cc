// expected-error: duplicate multi-flag member
#include <uarch/core/bits/bit_word.hpp>
struct Spec {
  using Word = std::uint64_t;
  bool a[[= uarch::bits::Flag{0}]];
};
constexpr auto kMask = uarch::bits::BitWord<Spec>::flag_mask<^^Spec::a, ^^Spec::a>();
