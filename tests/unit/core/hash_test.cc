#include <array>
#include <uarch/core/hash.hpp>
using namespace uarch::hash;
static_assert(fnv1a64("") == kFnv1a64OffsetBasis);
static_assert(fnv1a64_literal("hello") == 0xa430d84680aabd0bULL);
static_assert(fnv1a64_extend(fnv1a64("he"), "llo") == fnv1a64("hello"));
constexpr std::array<std::uint8_t, 3> kHigh{0, 128, 255};
constexpr std::array<std::byte, 3> kBytes{std::byte{0}, std::byte{128}, std::byte{255}};
static_assert(fnv1a64(kHigh) == fnv1a64(kBytes));
static_assert(fnv1a64_literal("a\0b") == fnv1a64(std::string_view{"a\0b", 3}));
int main() { return fnv1a64("hello") != 0xa430d84680aabd0bULL; }
