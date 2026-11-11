// expected-error: no matching
#include <uarch/core/bits/bit_word.hpp>

enum class Mode : std::uint8_t { kIdle, kReady };
enum class Color : std::uint8_t { kBlue };
struct StatusSpec {
  using Word = std::uint64_t;
  [[= uarch::bits::Field{0, 2}]] Mode mode;
};
using Status = uarch::bits::BitWord<StatusSpec>;
auto rejected = Status{}.with<^^StatusSpec::mode>(Color::kBlue);
