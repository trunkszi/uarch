#include <uarch/core/bits/bit_pattern.hpp>

struct Spec {
  using Word = std::uint8_t;
  [[= uarch::bits::Flag{0}]] bool ready;
  [[= uarch::bits::Field{1, 3}]] std::uint8_t value;
};
int main() {
  using namespace uarch::bits;
  using Pattern = BitPattern<Spec>;
  using Patch = BitPatch<Spec>;
  auto ready = Pattern{}.flag_set<^^Spec::ready>();
  auto clear = Pattern{}.flag_clear<^^Spec::ready>();
  auto conflict = ready.try_and(clear);
  if (conflict || conflict.error() != BitError::kConflict) return 1;
  if (!ready.try_and(ready) || !Pattern{}.try_and(clear)) return 2;
  auto high = Pattern{}.try_field_eq<^^Spec::value>(8);
  if (high || high.error() != BitError::kValueOverflow) return 3;
  auto write_overflow = Patch{}.try_set_field<^^Spec::value>(8);
  if (write_overflow || write_overflow.error() != BitError::kValueOverflow) return 4;
  if (Pattern{}.try_field_eq<^^Spec::value>(std::uint64_t{256}) || Patch{}.try_set_field<^^Spec::value>(std::uint64_t{256}) ||
      Pattern{}.try_field_eq<^^Spec::value>(-1) || Patch{}.try_set_field<^^Spec::value>(-1))
    return 12;
  auto set = Patch{}.set_flag<^^Spec::ready>(true);
  auto reset = Patch{}.set_flag<^^Spec::ready>(false);
  if (set.try_and(reset) || !set.try_and(set)) return 5;
  for (unsigned raw = 0; raw < 256; ++raw) {
    auto word = BitWord<Spec>::from_raw(static_cast<std::uint8_t>(raw));
    for (unsigned a = 0; a < 8; ++a) {
      auto value = static_cast<std::uint8_t>(a);
      auto match = ready.field_eq<^^Spec::value>(value);
      if (match.matches(word) != ((raw & 1) != 0 && ((raw >> 1) & 7) == a)) return 6;
      for (unsigned b = 0; b < 8; ++b) {
        auto lhs = Patch{}.set_field<^^Spec::value>(value);
        auto rhs = Patch{}.set_field<^^Spec::value>(static_cast<std::uint8_t>(b));
        if (lhs.then(rhs).apply(word) != rhs.apply(lhs.apply(word))) return 7;
        auto merged =
            Pattern{}.field_eq<^^Spec::value>(value).try_and(Pattern{}.field_eq<^^Spec::value>(static_cast<std::uint8_t>(b)));
        if (merged.has_value() != (a == b)) return 8;
      }
      auto patched = set.set_field<^^Spec::value>(value).apply(word);
      if (patched.raw() != ((raw & 240U) | 1U | (a << 1))) return 9;
    }
    if (set.then(reset).apply(word).test<^^Spec::ready>()) return 10;
    if (!reset.then(set).apply(word).test<^^Spec::ready>()) return 11;
  }
  return 0;
}
