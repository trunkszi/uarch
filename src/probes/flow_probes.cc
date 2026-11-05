#include <uarch/core/flow/cold.hpp>
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
