// expected-error: Delegate does not own the target
#include <uarch/core/flow/delegate.hpp>
struct T {
  void f(int) noexcept {}
};
auto bad = uarch::flow::bind<&T::f>(T{});
