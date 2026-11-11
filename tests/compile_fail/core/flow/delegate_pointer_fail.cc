// expected-error: pass a stable object lvalue
#include <uarch/core/flow/delegate.hpp>
struct T {
  void f(int) noexcept {}
};
T t;
auto bad = uarch::flow::bind<&T::f>(&t);
