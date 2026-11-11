#include <bit>
#include <uarch/tiers/tier_id.hpp>

using namespace uarch::tiers;
static_assert(all_tiers().size() == 5);
static_assert(std::to_underlying(TierId::Scalar) == 0 && std::to_underlying(TierId::Aarch64Neon) == 4);
static_assert(as_str(TierId::X86V3SlowPdep) == "x86_v3_slow_pdep");
static_assert(from_str("x86_v4") == TierId::X86V4);
static_assert(max_vector_bits(TierId::Scalar) == 64);
static_assert(vector_bytes(TierId::Scalar) == 0 && vector_bytes(TierId::Aarch64Neon) == 16);
static_assert(meta_of(TierId::Scalar)->vector_bytes == 0);
static_assert(meta_of(TierId::X86V3)->max_vector_bits == 256);
static_assert(target_cpu(TierId::Scalar) == "generic" && target_arch(TierId::Scalar) == "generic");
static_assert(target_cpu(TierId::X86V3) == "x86-64-v3" && target_arch(TierId::X86V3) == "x86_64");
static_assert(target_cpu(TierId::X86V3SlowPdep) == "x86-64-v3" && target_arch(TierId::X86V3SlowPdep) == "x86_64");
static_assert(target_cpu(TierId::X86V4) == "x86-64-v4" && target_arch(TierId::X86V4) == "x86_64");
static_assert(target_cpu(TierId::Aarch64Neon) == "generic" && target_arch(TierId::Aarch64Neon) == "aarch64");
static_assert(!target_cpu(static_cast<TierId>(255)) && !target_arch(static_cast<TierId>(255)));
static_assert(!vector_bytes(static_cast<TierId>(255)) && !max_vector_bits(static_cast<TierId>(255)) &&
              !has_fast_pdep(static_cast<TierId>(255)));
static_assert(has_fast_pdep(TierId::X86V3) == true && has_fast_pdep(TierId::X86V3SlowPdep) == false);
static_assert(!meta_of(static_cast<TierId>(255)) && !as_str(static_cast<TierId>(255)));
static_assert(!from_str("X86_V4"));
static_assert(uarch::reflect::freeze_string(*as_str(TierId::X86V4)) == "x86_v4");
static_assert(std::popcount(meta_of(TierId::X86V3)->feature_flags) == 16);
static_assert(std::popcount(meta_of(TierId::X86V4)->feature_flags) == 21);
static_assert((meta_of(TierId::X86V3SlowPdep)->feature_flags & std::to_underlying(Feature::kLahfLm)) != 0);
int main() { return 0; }
