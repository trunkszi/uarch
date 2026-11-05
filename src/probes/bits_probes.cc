#include <uarch/core/bits/bit_pattern.hpp>
#include <uarch/core/views/bit_ones.hpp>
struct Spec {
  using Word = std::uint64_t;
  std::uint8_t low[[= uarch::bits::Field{0, 4}]];
  std::uint8_t high[[= uarch::bits::Field{60, 4}]];
};
extern "C" [[gnu::noinline]] bool bitword_low_matches(std::uint64_t raw) {
  return uarch::bits::BitPattern<Spec>{}.field_eq<^^Spec::low>(5).matches(uarch::bits::BitWord<Spec>::from_raw(raw));
}
extern "C" [[gnu::noinline]] bool bitword_high_matches(std::uint64_t raw) {
  return uarch::bits::BitPattern<Spec>{}.field_eq<^^Spec::high>(5).matches(uarch::bits::BitWord<Spec>::from_raw(raw));
}
#if defined(__x86_64__)
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint32_t bit_ones_sum(std::uint64_t raw) {
  std::uint32_t sum = 0;
  for (auto bit : raw | uarch::views::ones) sum += bit;
  return sum;
}
#endif

struct PatchSpec {
  using Word = std::uint64_t;
  std::uint8_t first[[= uarch::bits::Field{0, 4}]];
  std::uint8_t second[[= uarch::bits::Field{8, 4}]];
};
extern "C" [[gnu::noinline]] std::uint64_t bitpatch_apply(std::uint64_t raw) {
  return uarch::bits::BitPatch<PatchSpec>{}
      .set_field<^^PatchSpec::first>(5)
      .set_field<^^PatchSpec::second>(9)
      .apply(uarch::bits::BitWord<PatchSpec>::from_raw(raw))
      .raw();
}
extern "C" [[gnu::noinline]] std::uint64_t bitmap64_pop_lowest(std::uint64_t raw) {
  auto bitmap = uarch::bits::Bitmap64::from_raw(raw);
  auto index = bitmap.pop_lowest_one();
  return bitmap.raw() + index.value_or(64U);
}
