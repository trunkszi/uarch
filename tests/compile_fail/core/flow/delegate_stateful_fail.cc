// expected-error: stateful closures are not owned
#include <uarch/core/flow/delegate.hpp>
void test() {
  int x = 3;
  auto bad = uarch::flow::bind([x](int y) noexcept { return x + y; });
}
