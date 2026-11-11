#include <array>
#include <string>
#include <uarch/core/bits/bitmap64.hpp>

int main() {
  using uarch::bits::Bitmap64;
  for (auto text : std::array<std::string_view, 5>{"2", " 1", "1x", "-1", "1\n"}) {
    if (Bitmap64::try_from_string(text)) return 1;
  }
  if (Bitmap64::try_from_string(std::string(65, '1'))) return 2;
  if (!Bitmap64::try_from_string("")->is_empty() || Bitmap64::try_from_string("10101")->raw() != 21) return 3;
  if (!Bitmap64::try_from_string(std::string(64, '1'))->is_full()) return 4;
  std::uint64_t seed = 1;
  for (unsigned sample = 0; sample < 2000; ++sample) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    auto bitmap = Bitmap64::from_raw(seed);
    if (Bitmap64::from_bitset(bitmap.to_bitset()) != bitmap) return 5;
    unsigned count = 0;
    for (std::uint32_t bit = 0; bit < 64; ++bit) {
      bool present = ((seed >> bit) & 1) != 0;
      count += present ? 1U : 0U;
      if (bitmap.test(bit) != present || !bitmap.set(bit).test(bit) || bitmap.clear(bit).test(bit) ||
          bitmap.toggle(bit).test(bit) == present)
        return 6;
      std::optional<std::uint32_t> next, prev;
      for (std::uint32_t i = bit; i < 64; ++i)
        if (((seed >> i) & 1) != 0) {
          next = i;
          break;
        }
      for (std::uint32_t i = bit + 1; i > 0; --i)
        if (((seed >> (i - 1)) & 1) != 0) {
          prev = i - 1;
          break;
        }
      if (bitmap.next_one_from(bit) != next || bitmap.prev_one_from(bit) != prev) return 7;
    }
    if (count != bitmap.count() || bitmap.next_one_from(64) || bitmap.prev_one_from(64) != bitmap.highest_one()) return 8;
    auto pop = bitmap;
    unsigned seen = 0;
    while (auto index = pop.pop_lowest_one()) {
      if (((seed >> *index) & 1) == 0) return 9;
      ++seen;
    }
    if (seen != count || !pop.is_empty() || pop.pop_highest_one()) return 10;
    pop = bitmap;
    seen = 0;
    while (pop.pop_highest_one()) ++seen;
    if (seen != count) return 11;
    auto other = Bitmap64::from_raw(~seed >> 1);
    if ((bitmap - other).raw() != (seed & ~other.raw()) || (bitmap & other).raw() != (seed & other.raw()) ||
        (bitmap | other).raw() != (seed | other.raw()) || (bitmap ^ other).raw() != (seed ^ other.raw()))
      return 12;
  }
  if (Bitmap64::all().lowest_zero() || Bitmap64::empty().lowest_one() || Bitmap64::empty().highest_one() ||
      Bitmap64::empty().clear_lowest_one().raw() != 0)
    return 13;
  for (unsigned bit = 0; bit < 64; ++bit) {
    const auto high = std::uint64_t{1} << bit;
    const auto value = Bitmap64::from_raw(high | (high - 1));
    if (value.highest_one_bit() != high || value.clear_highest_one().raw() != high - 1) return 14;
  }
  return 0;
}
