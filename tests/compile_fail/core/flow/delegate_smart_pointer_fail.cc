// expected-error: pass a stable object lvalue
#include <memory>
#include <uarch/core/flow/delegate.hpp>
struct T {
  void f(int) noexcept {}
};
auto t = std::make_unique<T>();
auto bad = uarch::flow::bind<&T::f>(t);
