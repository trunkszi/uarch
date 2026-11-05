#include <array>
#include <uarch/core/flow/reduce_lanes.hpp>
template <std::size_t N>
bool check() {
  std::array<int, 17> xs{};
  for (int i = 0; i < 17; ++i) xs[static_cast<std::size_t>(i)] = i + 1;
  for (std::size_t n = 0; n <= 17; ++n) {
    int seen = 0;
    auto step = [&](int a, int x) {
      ++seen;
      return a + x;
    };
    auto combine = [](int a, int b) { return a + b; };
    auto sum = uarch::flow::reduce_lanes<N>(std::span<const int>(xs.data(), n), 100, 0, step, combine);
    if (sum != 100 + static_cast<int>(n * (n + 1) / 2) || seen != static_cast<int>(n)) return false;
  }
  return true;
}
int main() { return !(check<2>() && check<4>() && check<8>()); }
