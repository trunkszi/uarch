#include <limits>
#include <uarch/core/flow/predicate.hpp>
#include <uarch/core/layout/register_passable.hpp>
using namespace uarch::flow;
static_assert(sizeof(Predicate) == 1);
static_assert(uarch::layout::NoPadding<Predicate>);
static_assert(uarch::layout::RegisterPassable<Predicate>);
static_assert(predicate(true).bit() == 1 && predicate(false).bit() == 0);
static_assert((predicate(true) & predicate(true)) == predicate(true));
static_assert((predicate(false) | predicate(true)) == predicate(true));
static_assert((predicate(true) ^ predicate(true)) == predicate(false));
static_assert((!predicate(false)) == predicate(true));
constexpr auto forged = std::bit_cast<Predicate>(std::uint8_t{2});
static_assert(forged.bit() == 1 && forged.as_bool());
static_assert((!forged).bit() == 0 && forged == predicate(true));
int main() {
  for (unsigned raw = 0; raw < 256; ++raw) {
    auto p = std::bit_cast<Predicate>(static_cast<std::uint8_t>(raw));
    auto canonical = predicate(raw != 0);
    if (p.bit() != canonical.bit() || p.as_bool() != canonical.as_bool() || p != canonical || (p <=> canonical) != 0 ||
        (!p) != (!canonical))
      return 2;
    for (bool b : {false, true}) {
      auto q = predicate(b);
      if ((p & q) != (canonical & q) || (p | q) != (canonical | q) || (p ^ q) != (canonical ^ q)) return 3;
    }
    if (p * std::int64_t{-7} != canonical * std::int64_t{-7} || p.select_sign_i64(9) != canonical.select_sign_i64(9) ||
        p.select_register(11, 22) != canonical.select_register(11, 22))
      return 4;
  }
  for (bool a : {false, true})
    for (bool b : {false, true}) {
      auto p = predicate(a), q = predicate(b);
      if ((p & q).as_bool() != (a && b) || (p | q).as_bool() != (a || b) || (p ^ q).as_bool() != (a != b) ||
          (!p).as_bool() == a)
        return 1;
      if (p * std::int64_t{-7} != (a ? -7 : 0) || p * std::uint8_t{255} != (a ? 255 : 0)) return 1;
      for (auto n : {std::int64_t{0}, std::int64_t{7}, std::numeric_limits<std::int64_t>::min()}) {
        auto expected = a ? n : std::bit_cast<std::int64_t>(std::uint64_t{0} - std::bit_cast<std::uint64_t>(n));
        if (p.select_sign_i64(n) != expected) return 1;
      }
    }
}
