#include <algorithm>
#include <array>
#include <ranges>
#include <uarch/core/views/bit_ones.hpp>

using uarch::views::BitOnesView;
using namespace uarch::views;
template <class Input, class Adaptor>
concept HasPipe = requires(Input input, Adaptor adaptor) { input | adaptor; };
static_assert(!HasPipe<std::uint64_t, decltype(std::views::iota)>);
static_assert(HasPipe<std::uint64_t, Ones> && HasPipe<std::bitset<64>, Ones> && HasPipe<uarch::bits::Bitmap64, Ones>);
static_assert(std::ranges::forward_range<BitOnesView> && std::ranges::sized_range<BitOnesView>);
static_assert(std::ranges::borrowed_range<BitOnesView> && std::ranges::view<BitOnesView>);
int main() {
  using uarch::views::ones;
  constexpr std::uint64_t kRaw = (std::uint64_t{1} << 63) | 0x89;
  constexpr std::array<std::uint32_t, 4> kExpected{0, 3, 7, 63};
  auto raw = kRaw | ones;
  auto bitmap = uarch::bits::Bitmap64::from_raw(kRaw) | ones;
  auto bitset = std::bitset<64>{kRaw} | ones;
  if (!std::ranges::equal(raw, kExpected) || !std::ranges::equal(bitmap, raw) || !std::ranges::equal(bitset, raw)) return 1;
  auto iterator = (std::uint64_t{0x89} | ones).begin();
  auto copy = iterator;
  if (*iterator++ != 0 || *iterator != 3 || *copy != 0 || *++copy != 3 || iterator != copy) return 2;
  auto pipeline = ones | std::views::drop(1) | std::views::take(2) | std::views::transform([](auto bit) { return bit + 10; });
  constexpr std::array<std::uint32_t, 2> kTransformed{13, 17};
  if (!std::ranges::equal(pipeline(kRaw), kTransformed)) return 3;
  if (!std::ranges::equal(
          (kRaw | ones) | std::views::drop(1) | std::views::take(2) | std::views::transform([](auto bit) { return bit + 10; }),
          kTransformed))
    return 8;
  if (std::ranges::distance(raw) != 4 || raw.size() != 4 || (std::uint64_t{0} | ones).size() != 0) return 4;
  std::uint64_t source = 3;
  auto snapshot = source | ones;
  source = 0;
  if (snapshot.size() != 2 || snapshot.begin() == snapshot.end()) return 5;
  for (unsigned value = 0; value < 256; ++value) {
    auto view = static_cast<std::uint64_t>(value) | ones;
    unsigned seen = 0;
    for (auto bit : view) {
      if (((value >> bit) & 1) == 0) return 6;
      ++seen;
    }
    if (seen != view.size()) return 7;
  }
  return 0;
}
