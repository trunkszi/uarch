// expected-error: tail padding
#include <cstdint>
#include <uarch/core/layout/isolated.hpp>
#include <uarch/core/layout/no_padding.hpp>
static_assert((uarch::layout::assert_no_padding<uarch::layout::Isolated<std::uint64_t>>(), true));
