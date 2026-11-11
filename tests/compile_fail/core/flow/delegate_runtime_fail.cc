// expected-error: runtime function pointers are not compile-time targets
#include <uarch/core/flow/delegate.hpp>
void f(int) noexcept {}
auto bad = uarch::flow::bind(&f);
