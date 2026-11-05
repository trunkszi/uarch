#include <uarch/detect/policy.hpp>

namespace detect_fixture {
uarch::detect::Snapshot IntelV4() {
  using uarch::detect::CpuidRegisters;
  uarch::detect::Snapshot s;
  s.basic = {7, 0x756e6547, 0x6c65746e, 0x49656e69};
  s.signature.eax = 0x000906e0;
  s.signature.ecx = (1U << 0) | (1U << 9) | (1U << 12) | (1U << 13) | (1U << 19) | (1U << 20) | (1U << 22) | (1U << 23) |
                    (1U << 26) | (1U << 27) | (1U << 28) | (1U << 29);
  s.structured.ebx = (1U << 3) | (1U << 5) | (1U << 8) | (1U << 16) | (1U << 17) | (1U << 28) | (1U << 30) | (1U << 31);
  s.extended.eax = 0x80000007;
  s.extended_features.ecx = (1U << 0) | (1U << 5) | (1U << 8);
  s.invariant.edx = 1U << 8;
  s.xcr0_value = 0xe6;
  return s;
}
void Identify(uarch::detect::Snapshot& src, std::string_view vendor, std::uint32_t signature) {
  auto pack = [&](std::size_t offset) {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
      result |= static_cast<std::uint32_t>(static_cast<unsigned char>(vendor[offset + i])) << (i * 8);
    return result;
  };
  src.basic.ebx = pack(0);
  src.basic.edx = pack(4);
  src.basic.ecx = pack(8);
  src.signature.eax = signature;
}
}  // namespace detect_fixture
#ifndef UARCH_TEST_SUPPORT_ONLY
static_assert(std::is_empty_v<uarch::detect::HardwareCpuid> && !std::is_trivially_copyable_v<uarch::detect::HardwareCpuid> &&
              std::is_trivially_copy_constructible_v<uarch::detect::HardwareCpuid> &&
              std::is_trivially_destructible_v<uarch::detect::HardwareCpuid>);
int main() {
  using namespace uarch::detect;
  auto source = detect_fixture::IntelV4();
  if (!v3_supported(source) || !v4_supported(source) || !prefetch_write_supported(source) || feature(source, "unknown"))
    return 1;
  for (const auto& requirement : detail::kRequirements) {
    auto changed = source;
    auto& reg = requirement.leaf == 1   ? changed.signature
                : requirement.leaf == 7 ? changed.structured
                                        : changed.extended_features;
    (requirement.ebx ? reg.ebx : reg.ecx) &= ~(1U << requirement.bit);
    if (feature(changed, requirement.name)) return 2;
    if ((std::to_underlying(requirement.id) & uarch::tiers::kX86V3Features) != 0 && v3_supported(changed)) return 3;
    if (v4_supported(changed)) return 4;
  }
  source.basic.eax = 6;
  if (v3_supported(source) || feature(source, "avx2")) return 5;
  source = detect_fixture::IntelV4();
  source.extended.eax = 0x80000000;
  if (feature(source, "lzcnt") || prefetch_write_supported(source)) return 6;
  for (auto state : std::array<std::uint64_t, 4>{0, 2, 4, 0x20}) {
    source = detect_fixture::IntelV4();
    source.xcr0_value = state;
    if (v3_supported(source)) return 7;
  }
  for (auto state : std::array<std::uint64_t, 4>{6, 0x26, 0x66, 0xa6}) {
    source = detect_fixture::IntelV4();
    source.xcr0_value = state;
    if (!v3_supported(source) || v4_supported(source)) return 8;
  }
  source = detect_fixture::IntelV4();
  source.signature.ecx &= ~(1U << 27);
  if (v3_supported(source)) return 9;
  if (family_model(0x00050650) != std::pair<std::uint16_t, std::uint16_t>{6, 0x55} || family_model(0x00800f10).first != 0x17 ||
      family_model(0x00f50540) != std::pair<std::uint16_t, std::uint16_t>{5, 4})
    return 10;
  source = detect_fixture::IntelV4();
  source.xcr0_value.reset();
  if (v3_supported(source)) return 12;
  source = detect_fixture::IntelV4();
  auto no_prefetch = source;
  no_prefetch.extended_features.ecx &= ~(1U << 8);
  if (prefetch_write_supported(no_prefetch) || same_isa(source, no_prefetch)) return 13;
  source = detect_fixture::IntelV4();
  source.extended_features.ecx &= ~1U;
  if (v3_supported(source) || classify(source) != uarch::tiers::TierId::Scalar) return 14;
  HardwareCpuid hardware;
  auto local = Snapshot::from_source(hardware);
  if (local.basic != hardware.cpuid(0, 0)) return 11;
  return 0;
}
#endif
