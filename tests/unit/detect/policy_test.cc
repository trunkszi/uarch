#include <algorithm>
#include <uarch/detect/policy.hpp>
namespace detect_fixture {
uarch::detect::Snapshot IntelV4();
}
int main() {
  using namespace uarch::detect;
  using enum uarch::tiers::TierId;
  auto full = detect_fixture::IntelV4();
  auto narrow = full;
  narrow.xcr0_value = 6;
  const DetectPolicy wide{WidePolicy::Allow512};
  const DetectPolicy strict{WidePolicy::Allow512, UnknownPdep::Conservative, ExecutionDomainPolicy::RequireHomogeneous};
  if (classify(full) != X86V3 || classify(full, wide) != X86V4 || classify(Snapshot{}) != Scalar) return 1;
  std::array domain{full, narrow};
  if (classify_domain<Snapshot>(domain, wide) != X86V3 || classify_domain<Snapshot>(domain, strict) != Scalar) return 2;
  std::ranges::reverse(domain);
  if (classify_domain<Snapshot>(domain, wide) != X86V3) return 3;
  domain = {full, full};
  if (classify_domain<Snapshot>(domain, strict) != X86V4) return 4;
  auto virtual_cpu = full;
  virtual_cpu.signature.ecx |= 1U << 31;
  domain = {full, virtual_cpu};
  if (classify_domain<Snapshot>(domain, wide) != X86V3SlowPdep) return 5;
  domain = {virtual_cpu, full};
  if (classify_domain<Snapshot>(domain, wide) != X86V3SlowPdep) return 6;
  domain = {full, Snapshot{}};
  if (classify_domain<Snapshot>(domain, wide) != Scalar || classify_domain<Snapshot>({}, wide) != Scalar) return 7;
  for (auto a : uarch::tiers::all_tiers())
    for (auto b : uarch::tiers::all_tiers()) {
      if (intersect(a, b) != intersect(b, a) || intersect(a, a) != a || intersect(a, Scalar) != Scalar) return 8;
    }
  if (intersect(Aarch64Neon, X86V3) != Scalar || intersect(X86V3SlowPdep, X86V4) != X86V3SlowPdep ||
      intersect(static_cast<uarch::tiers::TierId>(255), X86V4) != Scalar)
    return 9;
  static_assert(classify(Snapshot{}) == Scalar);
  return 0;
}
