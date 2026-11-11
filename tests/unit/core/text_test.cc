#include <array>
#include <uarch/core/layout/register_passable.hpp>
#include <uarch/core/text.hpp>
using namespace uarch::text;
static_assert(sizeof(FixedAscii<16>) == 16 && uarch::layout::NoPadding<FixedAscii<16>>);
static_assert(uarch::layout::GprPair<FixedAscii<16>>);
consteval bool TextChecks() {
  auto text = FixedAscii<8>::try_from("hello");
  if (!text || text->as_string_view() != "hello" || text->size() != 5 || text->as_bytes()[7] != 0) return false;
  if (text->fnv1a64() != uarch::hash::fnv1a64("hello")) return false;
  if (FixedAscii<2>::try_from("abc").error() != AsciiError::TooLong) return false;
  if (FixedAscii<8>::try_from(std::string_view{"a\0b", 3}).error() != AsciiError::EmbeddedNul) return false;
  if (FixedAscii<8>::try_from(std::string_view{"\x80", 1}).error() != AsciiError::NonAscii) return false;
  if (FixedAscii<2>::from_truncating("abcd").as_string_view() != "ab") return false;
  if (FixedAscii<8>::from_truncating(std::string_view{"a\0b", 3}).as_string_view() != "a") return false;
  if (!FixedAscii<1>::empty().is_empty()) return false;
  return FixedAscii<8>::from_truncating("a") < FixedAscii<8>::from_truncating("b");
}
static_assert(TextChecks());
template <std::size_t N>
int CompareChecks() {
  for (unsigned i = 0; i < 128; ++i) {
    for (unsigned j = 0; j < 128; ++j) {
      FixedAscii<N> a{}, b{};
      a.bytes[N - 1] = static_cast<char>(i);
      b.bytes[N - 1] = static_cast<char>(j);
      if ((a == b) != (i == j) || (a < b) != (i < j)) return 1;
      a.bytes[0] = static_cast<char>(255);
      if (i != 255 && !(a > b)) return 2;
    }
  }
  return 0;
}
int main() {
  const std::array<std::uint8_t, 2> input{65, 66};
  const std::array<std::byte, 2> bytes{std::byte{65}, std::byte{66}};
  auto a = FixedAscii<4>::try_from(input);
  auto b = FixedAscii<4>::try_from(bytes);
  if (!a || !b || *a != *b || a->as_trimmed_bytes().size() != 2) return 1;
  if (FixedAscii<4>::from_truncating(input) != *a || FixedAscii<4>::from_truncating(bytes) != *b) return 2;
  return CompareChecks<1>() + CompareChecks<7>() + CompareChecks<8>() + CompareChecks<9>() + CompareChecks<16>() +
         CompareChecks<17>();
}
