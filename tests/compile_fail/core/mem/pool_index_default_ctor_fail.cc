// expected-error: slot 0 is the pool sentinel
#include <uarch/core/mem/pool_index.hpp>
uarch::mem::PoolIndex<int> rejected;
