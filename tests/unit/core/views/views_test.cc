#include <array>
#include <forward_list>
#include <uarch/core/views/chunk.hpp>
#include <uarch/core/views/enumerate.hpp>
#include <uarch/core/views/lanes.hpp>
#include <vector>
int main() {
  using namespace uarch::views;
  std::array<int, 5> xs{1, 2, 3, 4, 5};
  int groups = 0;
  for (auto group : xs | lanes<2>) {
    ++groups;
    for (int& x : group) x *= 2;
  }
  if (groups != 3 || xs[4] != 10) return 1;
  for (auto [i, x] : xs | enumerate) {
    if (x != 2 * (i + 1)) return 1;
    x += 1;
  }
  auto owned = std::vector<int>{1, 2, 3} | chunk(2);
  int sum = 0;
  for (auto group : owned)
    for (int x : group) sum += x;
  if (sum != 6) return 1;
  auto composed = xs | chunk(2) | enumerate;
  int count = 0;
  for (auto [i, part] : composed) {
    (void)i;
    count += static_cast<int>(std::ranges::distance(part));
  }
  if (count != 5) return 1;
  std::array<int, 0> empty{};
  if (!std::ranges::empty(empty | lanes<4>)) return 1;
  std::forward_list<int> linked{1, 2, 3};
  int linked_sum = 0;
  for (auto group : linked | chunk(8))
    for (int x : group) linked_sum += x;
  static_assert(!std::ranges::borrowed_range<decltype(owned)>);
  return linked_sum != 6;
}
