#include <cstdint>
#include <uarch/core/layout/no_padding.hpp>

struct Pair {
  std::uint64_t lo;
  std::uint64_t hi;
};
struct Gap {
  std::uint8_t first;
  std::uint64_t last;
};
struct Empty {};
struct Shared {
  [[no_unique_address]] Empty empty;
  std::uint64_t value;
};
struct Separate {
  Empty empty;
  std::uint64_t value;
};
struct Bits {
  unsigned bits : 3;
};
union Union {
  int value;
  float floating;
};
struct Boolean {
  bool value;
};
struct Base {
  int base;
};
struct Derived : Base {
  int next;
};
struct EmptyBase : Empty {
  std::uint64_t value;
};
enum Legacy : unsigned { Zero };
enum class Mode : std::uint16_t { Zero };
static_assert(uarch::layout::NoPadding<Pair>);
static_assert(uarch::layout::NoPadding<Shared>);
static_assert(uarch::layout::NoPadding<std::uint64_t[2]>);
static_assert(uarch::layout::NoPadding<Mode>);
static_assert(uarch::layout::NoPadding<Legacy>);
static_assert(!uarch::layout::NoPadding<EmptyBase>);
static_assert(uarch::layout::NoPadding<int*>);
static_assert(!uarch::layout::NoPadding<Gap>);
static_assert(!uarch::layout::NoPadding<Separate>);
static_assert(!uarch::layout::NoPadding<Boolean>);
static_assert(!uarch::layout::NoPadding<Bits>);
static_assert(!uarch::layout::NoPadding<Union>);
static_assert(!uarch::layout::NoPadding<Derived>);
static_assert(!uarch::layout::NoPadding<Gap[2]>);
static_assert(!uarch::layout::NoPadding<long double>);
constexpr auto report = uarch::layout::padding_report<Gap>();
consteval bool contains(std::string_view text, std::string_view needle) {
  if (needle.size() > text.size()) return false;
  for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
    bool equal = true;
    for (std::size_t j = 0; j < needle.size(); ++j) equal = equal && text[i + j] == needle[j];
    if (equal) return true;
  }
  return false;
}
static_assert(contains(report, "Gap"));
static_assert(contains(report, "last"));
static_assert(contains(report, "8"));
#if defined(__FLT16_MANT_DIG__)
struct HasFp16 {
  _Float16 value;
};
static_assert(!uarch::layout::NoPadding<_Float16>);
static_assert(!uarch::layout::NoPadding<HasFp16>);
#endif
#if defined(__clang__)
static_assert(!uarch::layout::NoPadding<__bf16>);
#endif
int main() { return report.empty(); }
