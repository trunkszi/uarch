// expected-error: target is not invocable on this receiver
#include <uarch/core/flow/delegate.hpp>
struct T {
  void f(int) noexcept {}
};
const T t{};
auto bad = uarch::flow::bind<&T::f>(t);
