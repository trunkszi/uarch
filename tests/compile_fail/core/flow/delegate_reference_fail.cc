// expected-error: result conversion would return a dangling reference
#include <uarch/core/flow/delegate.hpp>
int f() noexcept { return 1; }
auto bad = uarch::flow::Callback<const int&()>::bind<&f>();
