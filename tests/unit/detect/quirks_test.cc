#include <uarch/detect/policy.hpp>
namespace detect_fixture {
uarch::detect::Snapshot IntelV4();
void Identify(uarch::detect::Snapshot&, std::string_view, std::uint32_t);
}  // namespace detect_fixture
int main() {
  using namespace uarch::detect;
  for (auto entry : std::array<std::pair<std::string_view, std::uint32_t>, 3>{
           {{"AuthenticAMD", 0x00800f10}, {"AuthenticAMD", 0x00600f01}, {"HygonGenuine", 0x00900f10}}}) {
    auto source = detect_fixture::IntelV4();
    detect_fixture::Identify(source, entry.first, entry.second);
    if (quirks::pdep_cost(source) != quirks::PdepCost::Slow ||
        classify(source, {WidePolicy::Allow512, UnknownPdep::AssumeFast}) != uarch::tiers::TierId::X86V3SlowPdep)
      return 1;
    source.signature.ecx |= 1U << 31;
    if (quirks::pdep_cost(source) != quirks::PdepCost::Slow) return 2;
  }
  for (auto signature : std::array<std::uint32_t, 3>{0x00a00f10, 0x00b00f10, 0x00c00f10}) {
    auto source = detect_fixture::IntelV4();
    detect_fixture::Identify(source, "AuthenticAMD", signature);
    if (quirks::pdep_cost(source) != quirks::PdepCost::Fast) return 3;
  }
  auto source = detect_fixture::IntelV4();
  detect_fixture::Identify(source, "GenuineIntel", 0x00050650);
  if (!quirks::avoid_wide(source) || classify(source, {WidePolicy::Allow512}) != uarch::tiers::TierId::X86V3) return 4;
  source = detect_fixture::IntelV4();
  source.signature.ecx |= 1U << 31;
  if (quirks::pdep_cost(source) != quirks::PdepCost::Unknown || classify(source) != uarch::tiers::TierId::X86V3SlowPdep)
    return 5;
  if (classify(source, {WidePolicy::Max256, UnknownPdep::AssumeFast}) != uarch::tiers::TierId::X86V3) return 6;
  if (classify(source, {WidePolicy::Allow512, UnknownPdep::AssumeFast}) != uarch::tiers::TierId::X86V4) return 9;
  detect_fixture::Identify(source, "UnknownCPU!!", 0);
  source.signature.ecx &= ~(1U << 31);
  if (quirks::pdep_cost(source) != quirks::PdepCost::Unknown) return 7;
  for (auto signature : std::array<std::uint32_t, 2>{0x00000f10, 0x00400f00}) {
    detect_fixture::Identify(source, "GenuineIntel", signature);
    if (quirks::pdep_cost(source) != quirks::PdepCost::Fast) return 8;
  }
  return 0;
}
