// expected-error: target is not invocable on this receiver
#include <uarch/core/flow/delegate.hpp>
struct T {
  void f(int) && noexcept {}
};
T t;
auto bad = uarch::flow::Callback<void(int)>::bind<&T::f>(t);
