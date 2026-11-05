#include <array>
#include <limits>
#include <memory>
#include <uarch/core/flow/select.hpp>
using namespace uarch::flow;
template <class T>
bool check() {
  for (unsigned i = 0; i < 10000; ++i) {
    volatile unsigned input = i;
    auto p = predicate((input & 1U) != 0);
    const T a = static_cast<T>(i) * T{17};
    const T b = static_cast<T>(std::numeric_limits<unsigned>::max()) - static_cast<T>(i);
    if (select_register(p, a, b) != (p.as_bool() ? a : b) || p.select_register(a, b) != select(p, a, b)) return false;
  }
  return true;
}
static_assert(!RegisterSelectable<std::uint16_t>);
int main() {
  auto selected = predicate(true).select(std::make_unique<int>(7), std::make_unique<int>(9));
  if (*selected != 7) return 1;
  return !(check<std::uint32_t>() && check<std::uint64_t>() && check<std::int32_t>() && check<std::int64_t>());
}
