// expected-error: constraint
#include <uarch/core/simd/vec.hpp>
uarch::simd::vec<std::int64_t, 3> invalid_lanes;
uarch::simd::vec<std::int64_t, 16> invalid_width;
