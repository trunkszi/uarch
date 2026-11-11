// expected-error: void callback must not silently discard a non-void return value
#include <uarch/core/flow/delegate.hpp>
int f() noexcept { return 1; }
auto bad = uarch::flow::Callback<void()>::bind<&f>();
