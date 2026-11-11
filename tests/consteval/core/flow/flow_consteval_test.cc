#include <array>
#include <uarch/core/flow/eager.hpp>
#include <uarch/core/flow/reduce_lanes.hpp>
#include <uarch/core/flow/select.hpp>
#include <uarch/core/views/lanes.hpp>
constexpr bool check() {
  std::array<int, 5> xs{1, 2, 3, 4, 5};
  auto plus = [](int a, int b) { return a + b; };
  int sum = 0;
  for (auto group : xs | uarch::views::lanes<2>)
    for (int x : group) sum += x;
  return sum == 15 && uarch::flow::reduce_four_way(std::span<const int>(xs), 7, 0, plus, plus) == 22;
}
static_assert(check());
static_assert(uarch::flow::predicate(true).select_register(3, 7) == 3);
static_assert(uarch::flow::eager_all(true, true));
int main() {}
