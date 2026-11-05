#include <uarch/core/bits/bit_pattern.hpp>
#include <uarch/core/views/bit_ones.hpp>

using namespace uarch::bits;
template <class W>
struct Full {
  using Word = W;
  [[= Field{0, sizeof(W) * 8}]] W whole;
};
template <class W>
consteval bool FullWidth() {
  using S = Full<W>;
  constexpr auto kMember = ^^S::whole;
  auto word = BitWord<S>::from_raw(static_cast<W>(~std::uint64_t{0}));
  if (word.template get<kMember>() != static_cast<W>(~std::uint64_t{0})) return false;
  if (word.template wrapping_add_field<kMember>(1).raw() != 0 || word.template wrapping_add_top_field<kMember>(1).raw() != 0)
    return false;
  return BitWord<S>{word.to_bitset()} == word && word.template with_const<kMember, 0>().raw() == 0 &&
         BitPattern<S>{}.template field_eq<kMember>(word.template get<kMember>()).matches(word) &&
         BitPatch<S>{}.template set_field<kMember>(0).apply(word).raw() == 0;
}
static_assert(FullWidth<std::uint8_t>() && FullWidth<std::uint16_t>() && FullWidth<std::uint32_t>() &&
              FullWidth<std::uint64_t>());
enum class State : std::uint8_t { kIdle, kReady = 3 };
struct Spec {
  using Word = std::uint64_t;
  [[= Flag{63}]] bool enabled;
  [[= Field{0, 2}]] State state;
};
constexpr auto kWord = BitWord<Spec>{}.with_const<^^Spec::enabled, true>().with_const<^^Spec::state, State::kReady>();
static_assert(kWord.raw() == ((std::uint64_t{1} << 63) | 3));
static_assert(kWord.get<^^Spec::state>() == State::kReady && kWord.test<^^Spec::enabled>());
static_assert(BitPattern<Spec>{}.flag_set<^^Spec::enabled>().field_eq<^^Spec::state>(State::kReady).matches(kWord));
static_assert(BitPatch<Spec>{}.set_flag<^^Spec::enabled>(false).apply(kWord).raw() == 3);
static_assert(Bitmap64::from_raw(1ULL << 63).lowest_one() == 63);
static_assert(Bitmap64::all().count() == 64 && Bitmap64::empty().lowest_zero() == 0);
static_assert(Bitmap64::try_from_string("101")->raw() == 5 && !Bitmap64::try_from_string("oops"));
consteval unsigned BitSum() {
  unsigned result = 0;
  for (auto bit : std::uint64_t{0x89} | uarch::views::ones) result += bit;
  return result;
}
static_assert(BitSum() == 10);
struct Missing {
  using Word = std::uint8_t;
  std::uint8_t missing;
};
struct BoolField {
  using Word = std::uint8_t;
  [[= Field{0, 1}]] bool invalid;
};
struct Narrow {
  using Word = std::uint64_t;
  [[= Field{0, 9}]] std::uint8_t invalid;
};
struct Zero {
  using Word = std::uint8_t;
  [[= Field{0, 0}]] std::uint8_t invalid;
};
struct CppBits {
  using Word = std::uint8_t;
  [[= Flag{0}]] unsigned invalid : 1;
};
struct Duplicate {
  using Word = std::uint8_t;
  [[ = Field{0, 1}, = Flag{0} ]] std::uint8_t invalid;
};
static_assert(audit_spec<Missing>().find("missing") != std::string::npos);
static_assert(audit_spec<BoolField>().find("invalid bit value") != std::string::npos);
static_assert(audit_spec<Narrow>().find("value width overflow") != std::string::npos);
static_assert(audit_spec<Zero>().find("outside word") != std::string::npos);
static_assert(audit_spec<CppBits>().find("C++ bitfield") != std::string::npos);
static_assert(audit_spec<Duplicate>().find("exactly one") != std::string::npos);
int main() { return 0; }
