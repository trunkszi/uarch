#include <uarch/core/bits/bit_value.hpp>

enum class Unsigned : std::uint16_t { kValue = 1 };
enum class Signed : int { kValue = 1 };
enum Plain : unsigned { kPlain = 1 };
using namespace uarch::bits;
static_assert(BitWordStorage<std::uint8_t> && BitWordStorage<std::uint16_t> && BitWordStorage<std::uint32_t> &&
              BitWordStorage<std::uint64_t>);
static_assert(!BitWordStorage<bool> && !BitWordStorage<int> && !BitWordStorage<Unsigned> &&
              !BitWordStorage<const std::uint8_t>);
static_assert(BitValue<Unsigned> && BitValue<std::uint64_t>);
static_assert(!BitValue<Signed> && !BitValue<Plain> && !BitValue<bool> && !BitValue<double> && !BitValue<int*>);
int main() { return 0; }
