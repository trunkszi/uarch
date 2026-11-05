// expected-error: each tier requires exactly one TierMeta annotation
#include <uarch/tiers/tier_id.hpp>

enum class BrokenTier : unsigned {
  Missing = 0,
  Duplicate[[ = uarch::tiers::TierMeta{"a", 8, 64, false, 0}, = uarch::tiers::TierMeta{"b", 8, 64, false, 0} ]] = 1
};
constexpr auto kRejected = uarch::tiers::detail::MakeMetadata<BrokenTier>();
