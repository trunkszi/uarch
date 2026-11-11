#include <uarch/core/bits/bit_pattern.hpp>
#include <uarch/core/bits/bit_word.hpp>

struct Small {
  using Word = std::uint8_t;
  [[= uarch::bits::Flag{0}]] bool ready;
  [[= uarch::bits::Field{1, 3}]] std::uint8_t value;
  [[= uarch::bits::Field{4, 4}]] std::uint8_t top;
};

enum class Mode : std::uint8_t { kIdle, kReady };
enum class Color : std::uint8_t { kBlue };
struct EnumSpec {
  using Word = std::uint8_t;
  [[= uarch::bits::Field{0, 2}]] Mode mode;
};
template <bool Expected, class Spec, std::meta::info M, class Input>
consteval bool FieldCallsMatch() {
  using W = uarch::bits::BitWord<Spec>;
  using P = uarch::bits::BitPattern<Spec>;
  using Patch = uarch::bits::BitPatch<Spec>;
  return (requires(W w, Input value) { w.template with<M>(value); } == Expected) &&
         (requires(W w, Input value) { w.template try_with<M>(value); } == Expected) &&
         (requires(P p, Input value) { p.template field_eq<M>(value); } == Expected) &&
         (requires(P p, Input value) { p.template try_field_eq<M>(value); } == Expected) &&
         (requires(Patch p, Input value) { p.template set_field<M>(value); } == Expected) &&
         (requires(Patch p, Input value) { p.template try_set_field<M>(value); } == Expected);
}
static_assert(FieldCallsMatch<true, EnumSpec, ^^EnumSpec::mode, Mode>());
static_assert(FieldCallsMatch<false, EnumSpec, ^^EnumSpec::mode, Color>());
static_assert(FieldCallsMatch<false, EnumSpec, ^^EnumSpec::mode, int>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, Color>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, bool>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, char>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, wchar_t>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, char8_t>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, char16_t>());
static_assert(FieldCallsMatch<false, Small, ^^Small::value, char32_t>());
static_assert(FieldCallsMatch<true, Small, ^^Small::value, int>() && FieldCallsMatch<true, Small, ^^Small::value, unsigned>() &&
              FieldCallsMatch<true, Small, ^^Small::value, std::uint8_t>() &&
              FieldCallsMatch<true, Small, ^^Small::value, std::int8_t>() &&
              FieldCallsMatch<true, Small, ^^Small::value, std::uint64_t>());
template <class Spec, std::meta::info M, auto Value>
concept CanWithConst = requires(uarch::bits::BitWord<Spec> word) { word.template with_const<M, Value>(); };
static_assert(CanWithConst<EnumSpec, ^^EnumSpec::mode, Mode::kReady>);
static_assert(!CanWithConst<EnumSpec, ^^EnumSpec::mode, Color::kBlue>);
static_assert(!CanWithConst<Small, ^^Small::value, 'A'> && !CanWithConst<Small, ^^Small::value, true>);

struct FlagsSpec {
  using Word = std::uint8_t;
  bool a[[= uarch::bits::Flag{0}]];
  bool b[[= uarch::bits::Flag{3}]];
  bool c[[= uarch::bits::Flag{7}]];
};
int main() {
  using Flags = uarch::bits::BitWord<FlagsSpec>;
  static_assert(Flags::flag_mask<^^FlagsSpec::a, ^^FlagsSpec::b, ^^FlagsSpec::c>() == 137);
  for (unsigned raw = 0; raw < 256; ++raw) {
    auto value = Flags::from_raw(static_cast<std::uint8_t>(raw));
    if (value.with_flags<^^FlagsSpec::a, ^^FlagsSpec::b>().raw() != (raw | 9)) return 20;
    if (value.without_flags<^^FlagsSpec::a, ^^FlagsSpec::b>().raw() != (raw & 246)) return 21;
    if (value.all_of<^^FlagsSpec::a, ^^FlagsSpec::b>() != ((raw & 9) == 9)) return 22;
    if (value.any_of<^^FlagsSpec::a, ^^FlagsSpec::b>() != ((raw & 9) != 0)) return 23;
    if (value.none_of<^^FlagsSpec::a, ^^FlagsSpec::b>() != ((raw & 9) == 0)) return 24;
    if (value.with_flags<^^FlagsSpec::a>().without_flags<^^FlagsSpec::a>().raw() != (raw & 254)) return 25;
  }

  using uarch::bits::BitWord;
  for (unsigned raw = 0; raw < 256; ++raw) {
    auto word = BitWord<Small>::from_raw(static_cast<std::uint8_t>(raw));
    if (word.get<^^Small::value>() != ((raw >> 1) & 7)) return 1;
    if (word.test<^^Small::ready>() != ((raw & 1) != 0)) return 2;
    for (unsigned value = 0; value < 256; ++value) {
      auto v = static_cast<std::uint8_t>(value);
      auto updated = word.with<^^Small::value>(v);
      if (updated.raw() != ((raw & ~14U) | ((value & 7) << 1))) return 3;
      auto checked = word.try_with<^^Small::value>(v);
      if (checked.has_value() != (value < 8)) return 4;
      if (word.wrapping_add_field<^^Small::value>(v).get<^^Small::value>() != (((raw >> 1) + value) & 7)) return 5;
      auto wrapped = word.wrapping_add_field<^^Small::value>(v);
      if ((wrapped.raw() & ~14U) != (raw & ~14U)) return 7;
      auto expected = (raw + (value << 4)) & 255;
      if (word.wrapping_add_top_field<^^Small::top>(v).raw() != expected) return 6;
    }
  }
  auto zero = BitWord<Small>{};
  if (zero.try_with<^^Small::value>(std::uint64_t{256}) || zero.try_with<^^Small::value>(-1)) return 8;
  if (zero.with<^^Small::value>(std::uint64_t{257}).raw() != 2) return 9;
  return 0;
}
