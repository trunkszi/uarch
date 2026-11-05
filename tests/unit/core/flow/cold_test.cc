#include <uarch/core/flow/cold.hpp>
constexpr int increment(int n) noexcept { return n + 1; }
static_assert(uarch::flow::cold_call<increment>(4) == 5);
int main() {
  using namespace uarch::flow;
  cold_path();
  return cold_call<increment>(4) != 5 || !likely(true) || unlikely(false);
}
