#include <uarch/core/flow/cold.hpp>
#include <uarch/core/flow/delegate.hpp>
#include <uarch/core/mem/pool_index.hpp>
#include <uarch/probes/delegate_targets.hpp>
using namespace uarch::flow;
extern "C" [[gnu::noinline]] std::uint64_t flow_select_register_u64(bool cond, std::uint64_t a, std::uint64_t b) {
  return select_register(predicate(cond), a, b);
}
extern "C" [[gnu::noinline]] std::uint64_t flow_select_u64(bool cond, std::uint64_t a, std::uint64_t b) {
  return select(predicate(cond), a, b);
}
extern "C" [[gnu::cold, gnu::noinline]] std::uint64_t cold_sink_fn(std::uint64_t x) { return x * 17 + 1; }
extern "C" [[gnu::cold, gnu::noinline]] std::uint64_t cold_sink_body(std::uint64_t x) { return cold_call<cold_sink_fn>(x); }
extern "C" [[gnu::noinline]] std::uint64_t flow_likely(bool c, std::uint64_t x) {
  if (likely(c)) return x;
  return cold_sink_body(x);
}
extern "C" [[gnu::noinline]] std::uint64_t flow_unlikely(bool c, std::uint64_t x) {
  if (unlikely(c)) return cold_sink_body(x);
  return x;
}

#include <uarch/probes/gate.hpp>
constexpr std::uint64_t pair_sum(uarch::probes::RegisterPair pair) noexcept { return pair.a + pair.b; }
extern "C" [[gnu::noinline]] std::uint64_t cold_invoke_pair(uarch::probes::RegisterPair pair) {
  return cold_invoke<pair_sum>(pair);
}

namespace uarch::probes {
using IntCallback = uarch::flow::Callback<int(int)>;
using Span = std::span<const int>;
using Index = uarch::mem::PoolIndex<struct DelegateIndex>;
DelegateNonprimary probe_receiver;
constexpr auto nonprimary_callback = IntCallback::bind<&DelegateTarget::integer>(probe_receiver);
struct Holder {
  IntCallback callback;
};
struct LargeArgument {
  std::uint64_t words[8];
};
#if defined(__clang__)
#pragma clang diagnostic push
// This ABI probe intentionally gives a register-returning C++ value a stable C symbol.
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif
extern "C" IntCallback delegate_identity(IntCallback callback) noexcept { return callback; }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
extern "C" {
int delegate_invoke_i32(IntCallback callback, int x) noexcept { return callback(x); }
int delegate_invoke_span(uarch::flow::Callback<int(Span)> callback, Span x) noexcept { return callback(x); }
int delegate_invoke_member(const Holder* holder, int x) noexcept { return holder->callback(x); }
int delegate_stub_i32(void* context, int x) noexcept {
  return IntCallback::bind<&DelegateTarget::integer>(probe_receiver).stub()(context, x);
}
int delegate_stub_span(void* context, Span x) noexcept {
  return uarch::flow::Callback<int(Span)>::bind<&DelegateTarget::span>(probe_receiver).stub()(context, x);
}
int delegate_stub_index(void* context, Index x) noexcept {
  return uarch::flow::Callback<int(Index)>::bind<&DelegateTarget::index>(probe_receiver).stub()(context, x);
}
int delegate_stub_nonprimary(void* context, int x) noexcept { return nonprimary_callback.stub()(context, x); }
int delegate_invoke_large_rref(uarch::flow::Callback<int(LargeArgument)> callback, LargeArgument&& x) noexcept {
  return callback(static_cast<LargeArgument&&>(x));
}
int delegate_checked_i32(IntCallback callback, int x) noexcept { return callback ? callback(x) : 0; }
}

}  // namespace uarch::probes
