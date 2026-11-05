#include <uarch/core/flow/eager.hpp>
int main() {
  using namespace uarch::flow;
  int n = 0;
  auto tick = [&](bool b) {
    ++n;
    return b;
  };
  if (eager_all(tick(false), tick(true)) || n != 2) return 1;
  n = 0;
  if (!eager_any(tick(true), tick(false)) || n != 2) return 1;
  return !eager_all() || eager_any();
}
