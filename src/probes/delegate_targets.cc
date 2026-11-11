#include <uarch/probes/delegate_targets.hpp>
namespace uarch::probes {
int DelegateTarget::integer(int x) const noexcept { return static_cast<int>(bias) + x; }
int DelegateTarget::span(DelegateSpan x) const noexcept { return static_cast<int>(x.size()) + static_cast<int>(bias); }
int DelegateTarget::index(DelegateIndexValue x) const noexcept { return static_cast<int>(x.slot()) + static_cast<int>(bias); }
}  // namespace uarch::probes
