#include <uarch/core/layout/abi_class.hpp>
#include <uarch/core/layout/cache_aligned.hpp>
#include <uarch/core/layout/no_padding.hpp>
using namespace uarch::layout;
struct Integers {
  std::uint64_t a, b;
};
struct Doubles {
  double a, b;
};
struct Floats {
  float a, b, c;
};
struct Mixed {
  double a;
  std::uint64_t b;
};
struct OneLane {
  float a;
  std::uint32_t b;
};
struct [[gnu::packed]] Packed {
  std::uint8_t a;
  std::uint64_t b;
};
struct Destructor {
  std::uint64_t a;
  ~Destructor() { a = 0; }
};
struct Copy {
  std::uint64_t a;
  Copy(const Copy& other) : a(other.a + 1) {}
};
static_assert(!NoPadding<Destructor> && !NoPadding<Copy>);
struct MoveOnly {
  std::uint64_t a;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) = default;
};
struct Extended {
  long double a;
};
struct Wide {
  __uint128_t a;
};
struct Empty {};
struct Shared {
  [[no_unique_address]] Empty empty;
  std::uint64_t a;
};
struct Separate {
  std::uint64_t a;
  Empty empty;
};
struct Bits {
  unsigned a : 1;
};
union Union {
  float a;
  int b;
};
struct Boolean {
  bool a;
};
struct Array {
  float values[4];
};
struct TooBig {
  double values[4];
};
static_assert(sysv_classify<Integers>() == AbiShape{AbiClass::Integer, AbiClass::Integer, 2});
static_assert(sysv_classify<Doubles>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 2});
static_assert(sysv_classify<Floats>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 2});
static_assert(aapcs64_classify<Floats>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 3});
static_assert(aapcs64_classify<Doubles>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 2});
static_assert(aapcs64_classify<Mixed>() == AbiShape{AbiClass::Integer, AbiClass::Integer, 2});
static_assert(sysv_classify<Mixed>() == AbiShape{AbiClass::Sse, AbiClass::Integer, 2});
static_assert(sysv_classify<OneLane>() == AbiShape{AbiClass::Integer, AbiClass::None, 1});
static_assert(sysv_classify<Wide>() == AbiShape{AbiClass::Integer, AbiClass::Integer, 2});
static_assert(sysv_classify<Shared>() == AbiShape{AbiClass::Integer, AbiClass::None, 1});
static_assert(sysv_classify<Separate>() == AbiShape{AbiClass::Integer, AbiClass::None, 1});
static_assert(sysv_classify<Boolean>() == AbiShape{AbiClass::Integer, AbiClass::None, 1});
static_assert(sysv_classify<Array>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 2});
static_assert(aapcs64_classify<Array>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 4});
static_assert(sysv_classify<Packed>().lo == AbiClass::Memory);
static_assert(aapcs64_classify<Packed>().lo == AbiClass::Memory);
static_assert(sysv_classify<Destructor>().lo == AbiClass::Memory);
static_assert(sysv_classify<Copy>().lo == AbiClass::Memory);
static_assert(sysv_classify<MoveOnly>().lo == AbiClass::Memory);
static_assert(sysv_classify<CacheAligned<std::uint64_t>>().lo == AbiClass::Memory);
static_assert(sysv_classify<Extended>().lo == AbiClass::Memory);
static_assert(sysv_classify<Bits>().lo == AbiClass::Memory);
static_assert(sysv_classify<Union>().lo == AbiClass::Memory);
static_assert(aapcs64_classify<TooBig>().lo == AbiClass::Memory);
struct alignas(16) OverAlignedFloat {
  float value;
};
struct EmptyAndDouble {
  Empty empty;
  double value;
};
static_assert(aapcs64_classify<OverAlignedFloat>() == AbiShape{AbiClass::Integer, AbiClass::Integer, 2});
static_assert(aapcs64_classify<EmptyAndDouble>() == AbiShape{AbiClass::Integer, AbiClass::Integer, 2});
struct NuaFloatPair {
  [[no_unique_address]] Empty empty;
  float a;
  float b;
};
static_assert(sizeof(NuaFloatPair) == 8);
static_assert(aapcs64_classify<NuaFloatPair>() == AbiShape{AbiClass::Sse, AbiClass::Sse, 2});
int main() {}
