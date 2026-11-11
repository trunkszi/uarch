// expected-error: noexcept callback requires a noexcept target and conversion
#include <uarch/core/flow/delegate.hpp>
auto bad = uarch::flow::Callback<int(int)>::bind<[](int x) { return x; }>();
