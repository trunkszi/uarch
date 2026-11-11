// expected-error: use try_push_back or unchecked_push_back
#include <uarch/core/container/inplace_vector.hpp>
void rejected() {
  uarch::container::freestanding::inplace_vector<int, 4> value;
  value.push_back(1);
}
