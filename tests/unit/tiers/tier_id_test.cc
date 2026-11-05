#include <array>
#include <bit>
#include <string_view>
#include <uarch/tiers/tier_id.hpp>

int main() {
  using namespace uarch::tiers;
  constexpr std::array<std::string_view, 5> kNames{"scalar", "x86_v3", "x86_v3_slow_pdep", "x86_v4", "aarch64_neon"};
  constexpr std::array<std::uint32_t, 5> kBits{64, 256, 256, 512, 128};
  constexpr std::array<std::uint32_t, 5> kBytes{0, 32, 32, 64, 16};
  constexpr std::array<std::string_view, 5> kCpus{"generic", "x86-64-v3", "x86-64-v3", "x86-64-v4", "generic"};
  constexpr std::array<std::string_view, 5> kArchs{"generic", "x86_64", "x86_64", "x86_64", "aarch64"};
  constexpr std::array<bool, 5> kFast{false, true, false, true, false};
  for (std::size_t i = 0; i < kNames.size(); ++i) {
    auto tier = static_cast<TierId>(i);
    auto meta = meta_of(tier);
    if (!meta || as_str(tier) != kNames[i] || from_str(kNames[i]) != tier || max_vector_bits(tier) != kBits[i] ||
        vector_bytes(tier) != kBytes[i] || target_cpu(tier) != kCpus[i] || target_arch(tier) != kArchs[i] ||
        has_fast_pdep(tier) != kFast[i] || all_tiers()[i] != tier)
      return 1;
    if (meta->name_len != kNames[i].size() || meta->name[meta->name_len] != '\0' ||
        std::string_view{meta->target_cpu, meta->target_cpu_len} != kCpus[i] ||
        std::string_view{meta->target_arch, meta->target_arch_len} != kArchs[i] ||
        meta->target_cpu[meta->target_cpu_len] != '\0' || meta->target_arch[meta->target_arch_len] != '\0')
      return 2;
  }
  for (unsigned i = 5; i <= 255; ++i) {
    auto tier = static_cast<TierId>(i);
    if (meta_of(tier) || as_str(tier) || max_vector_bits(tier) || vector_bytes(tier) || has_fast_pdep(tier) ||
        target_cpu(tier) || target_arch(tier))
      return 3;
  }
  for (auto name : std::array<std::string_view, 5>{"", "X86V3", "x86_v3 ", "x86", "abcdefghijklmnopqrstuvwxyz0123456789"}) {
    if (from_str(name)) return 4;
  }
  if (from_str(std::string_view{"scalar\0junk", 11})) return 5;
  if (std::popcount(meta_of(TierId::X86V3)->feature_flags) != 16 ||
      std::popcount(meta_of(TierId::X86V4)->feature_flags) != 21 ||
      meta_of(TierId::X86V3)->feature_flags != meta_of(TierId::X86V3SlowPdep)->feature_flags ||
      meta_of(TierId::Scalar)->feature_flags != 0 ||
      meta_of(TierId::Aarch64Neon)->feature_flags != std::to_underlying(Feature::kNeon))
    return 6;
  if (!(meta_of(TierId::X86V3)->feature_flags & std::to_underlying(Feature::kLahfLm)) ||
      !(meta_of(TierId::X86V4)->feature_flags & std::to_underlying(Feature::kLahfLm)))
    return 7;
  return 0;
}
