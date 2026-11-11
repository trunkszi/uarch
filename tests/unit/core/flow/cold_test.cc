#include <uarch/core/flow/cold.hpp>
#include <uarch/core/mem/prefetch.hpp>
constexpr int increment(int n) noexcept { return n + 1; }
static_assert(uarch::flow::cold_call<increment>(4) == 5);
void RuntimeAdapters();
int main() {
  RuntimeAdapters();
  using namespace uarch::flow;
  cold_path();
  return cold_call<increment>(4) != 5 || !likely(true) || unlikely(false);
}

static_assert(uarch::flow::cold_invoke<increment>(4) == 5);
static_assert(std::is_same_v<uarch::flow::ColdArg<int>, int>);
struct Large {
  std::uint64_t words[8];
};
static_assert(std::is_same_v<uarch::flow::ColdArg<Large>, const Large&>);

static_assert(std::is_same_v<uarch::flow::ColdArg<uarch::mem::PrefetchWrite>, uarch::mem::PrefetchWrite>);
constexpr int large_first(const Large& value) noexcept { return static_cast<int>(value.words[0]); }
static_assert(uarch::flow::cold_invoke<large_first>(Large{{9}}) == 9);
static_assert(noexcept(uarch::flow::cold_invoke<increment>(1)));

void RuntimeAdapters() {
  Large large{{9}};
  if (uarch::flow::cold_invoke<large_first>(large) != 9) std::abort();
}
