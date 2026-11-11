// expected-error: noexcept callback requires a noexcept target and conversion
#include <uarch/core/flow/delegate.hpp>
struct R {
  operator int() const noexcept(false) { return 1; }
};
R f() noexcept { return {}; }
auto bad = uarch::flow::Callback<int()>::bind<&f>();
