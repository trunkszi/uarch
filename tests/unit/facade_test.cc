#include <uarch/uarch.hpp>
struct Sample {
  std::uint64_t timestamp;
  std::uint64_t count;
};
struct Header {
  using Word = std::uint64_t;
  bool ready[[= uarch::bits::Flag{0}]];
  std::uint8_t count[[= uarch::bits::Field{1, 7}]];
};
static_assert(uarch::NoPadding<Sample> && uarch::RegisterPassable<Sample>);
static_assert(uarch::Arch<uarch::Scalar>);
int main() {
  using namespace uarch::prelude;
  auto callback = bind([](int x) noexcept { return x + 1; });
  static_assert(std::is_same_v<decltype(callback), Callback<int(int)>>);
  static_assert(std::is_same_v<CallArg<int>, int>);
  if (callback(3) != 4) return 13;
  static_assert(fnv1a64_literal("hello") == 0xa430d84680aabd0bULL);
  static_assert(L1 == Locality::High && L2 == Locality::Moderate && L3 == Locality::Low);
  static_assert(assert_layout<Sample, 16, 8, 0, 8>());
  static_assert(std::is_same_v<ColdArg<Sample>, Sample>);
  static_assert(std::is_same_v<DetectPolicy, uarch::detect::DetectPolicy>);
  static_assert(std::is_same_v<WidePolicy, uarch::detect::WidePolicy>);
  static_assert(std::is_same_v<ExecutionDomainPolicy, uarch::detect::ExecutionDomainPolicy>);
  static_assert(std::is_same_v<UnknownPdep, uarch::detect::UnknownPdep>);
  auto host = detect();
  if (host && !uarch::tiers::as_str(host->id())) return 4;
  if (FixedAscii<8>::from_truncating("name").as_string_view() != "name") return 5;
  if (divisor_u32<3>().div(9) != 3 || divisor_u64<3>().div(9) != 3) return 6;
  if (saturating_add(std::uint64_t{4}, std::uint64_t{5}) != 9 || checked_mul(3, 4) != 12) return 7;
  if (!eager_all(true, true) || eager_any(false, false) || select(predicate(true), 1, 2) != 1) return 8;
  freestanding::inplace_vector<int, 2> small;
  if (!small.try_resize(1, 9) || small.front() != 9) return 9;
  constexpr auto kLanes = vec<std::uint32_t, 4>::from_array(std::array<std::uint32_t, 4>{1, 2, 3, 4});
  static_assert(reduce_min(kLanes) == 1 && reduce_max(kLanes) == 4);
  constexpr auto kMask = BitWord<Header>::flag_mask<^^Header::ready>();
  static_assert(kMask == 1);
  unsigned bits = 0;
  for (auto bit : 9ULL | views::ones) bits += bit;
  if (bits != 3) return 12;
  if (!uarch::tiers::as_str(uarch::tiers::compiled_tier())) return 10;
  SoaColumns<Sample, 4> columns;
  columns.set_row(1, Sample{7, 9});
  auto divisor = FastDivisorU64::try_new(3);
  if (!divisor || divisor->div(columns.row(1).count) != 3) return 1;
  auto selected = select_register(predicate(true), columns.row(1).timestamp, std::uint64_t{0});
  auto word = BitWord<Header>{};
  auto bitmap = Bitmap64{};
  uarch::container::inplace_vector<int, 4> vector;
  if (vector.try_push_back(7) == nullptr) return 1;
  if (selected != 7 || word.raw() != 0 || bitmap.raw() != 0) return 1;
  uarch::Scalar scalar;
  return scalar.bit_gather(0xA0, 0xF0) != 10;
}
