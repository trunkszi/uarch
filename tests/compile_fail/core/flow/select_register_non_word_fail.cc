// expected-error: no matching function
#include <uarch/core/flow/select.hpp>
int main() { return static_cast<int>(uarch::flow::select_register(uarch::flow::predicate(true), 1.0, 2.0)); }
