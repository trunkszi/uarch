// expected-error: member 'tail' at byte 8
#include <cstdint>
#include <uarch/core/layout/no_padding.hpp>
struct Padded {
  std::uint8_t head;
  std::uint64_t tail;
};
consteval void prove() { uarch::layout::assert_no_padding<Padded>(); }
static_assert((prove(), true));
