/** @file @brief Hosted sample linking the facade and the detection library. */
#include <cstdint>
#include <uarch/uarch.hpp>

int main() {
  const auto chosen = uarch::select_register(uarch::predicate(true), std::uint64_t{1}, std::uint64_t{0});
  const auto other = uarch::select_register(uarch::predicate(false), std::uint64_t{1}, std::uint64_t{0});
  const auto described = uarch::tiers::as_str(uarch::tiers::compiled_tier());
  using namespace uarch::prelude;
  const auto detected = detect();
  const auto label = FixedAscii<16>::try_from("worker");
  if (!label || label->fnv1a64() != fnv1a64_literal("worker")) return 2;
  if (chosen != std::uint64_t{1} || other != std::uint64_t{0} || !described) return 1;
  if (detected && !uarch::tiers::as_str(detected->id())) return 1;
  return 0;
}
