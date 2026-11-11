// expected-error: reduce_lanes lane count must be 2, 4, or 8
#include <uarch/core/flow/reduce_lanes.hpp>
int main() {
  auto plus = [](int a, int b) { return a + b; };
  return uarch::flow::reduce_lanes<3>(std::span<const int>{}, 0, 0, plus, plus);
}
