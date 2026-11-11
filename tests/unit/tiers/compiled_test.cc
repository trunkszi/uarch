#include <uarch/tiers/compiled.hpp>

#ifndef UARCH_EXPECTED_TIER
#error "compiled_test requires an independently configured expected tier"
#endif
static_assert(std::to_underlying(uarch::tiers::compiled_tier()) == UARCH_EXPECTED_TIER);
#if defined(__x86_64__) && !defined(__LAHF_SAHF__)
static_assert(uarch::tiers::compiled_tier() == uarch::tiers::TierId::Scalar);
#endif
int main() { return !uarch::tiers::meta_of(uarch::tiers::compiled_tier()); }
