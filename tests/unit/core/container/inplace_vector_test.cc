#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <uarch/core/container/inplace_vector.hpp>

using uarch::container::inplace_vector;
struct Tracked {
  static inline int constructions = 0, destructions = 0, live = 0;
  int value;
  explicit Tracked(int x) noexcept : value(x) {
    ++constructions;
    ++live;
  }
  Tracked(const Tracked& other) noexcept : Tracked(other.value) {}
  Tracked(Tracked&& other) noexcept : Tracked(other.value) { other.value = -1; }
  ~Tracked() noexcept {
    ++destructions;
    --live;
  }
  auto operator<=>(const Tracked&) const noexcept = default;
};
struct alignas(128) Aligned {
  int value;
  explicit Aligned(int v) noexcept : value(v) {}
  ~Aligned() noexcept { value = 0; }
};
static_assert(std::ranges::contiguous_range<inplace_vector<int, 8>> && std::ranges::sized_range<inplace_vector<int, 8>>);
static_assert(std::ranges::contiguous_range<const inplace_vector<Tracked, 4>>);
static_assert(std::is_empty_v<inplace_vector<Tracked, 0>> && std::is_trivially_copyable_v<inplace_vector<Tracked, 0>>);
static_assert(std::is_trivially_copyable_v<inplace_vector<int, 0>>);
static_assert(std::is_trivially_destructible_v<inplace_vector<int, 0>>);
static_assert(std::is_trivially_copyable_v<inplace_vector<int, 4>>);
static_assert(std::is_trivially_destructible_v<inplace_vector<int, 4>>);
static_assert(!std::is_trivially_copyable_v<inplace_vector<Tracked, 4>>);
static_assert(!std::is_trivially_destructible_v<inplace_vector<Tracked, 4>>);
constexpr auto kValues = []() {
  inplace_vector<int, 4> v;
  v.try_push_back(3);
  v.unchecked_push_back(7);
  v.try_emplace_back(9);
  v.pop_back();
  return v;
}();
static_assert(kValues.size() == 2 && kValues.front() == 3 && kValues.back() == 7 && kValues.data()[1] == 7);
consteval bool ConstexprOperations() {
  inplace_vector<int, 3> v;
  v.unchecked_emplace_back(4);
  v.try_push_back(2);
  v.unchecked_push_back(3);
  if (!v.full() || v.try_push_back(8) != nullptr) return false;
  auto copy = v;
  inplace_vector<int, 3> assigned;
  assigned = copy;
  auto moved = copy;
  assigned = moved;
  if (assigned != v || (assigned <=> v) != 0 || assigned.end() - assigned.begin() != 3) return false;
  assigned.pop_back();
  assigned.clear();
  assigned.try_emplace_back(6);
  inplace_vector<Tracked, 0> zero;
  zero.clear();
  return assigned.size() == 1 && assigned.front() == 6 && zero.data() == nullptr && zero.begin() == zero.end() &&
         !zero.try_emplace_back(1);
}
static_assert(ConstexprOperations());
// Keep the standard-library heap-sort fallback opaque to GCC's small-array bounds heuristic.
[[gnu::noinline]] void SortRange(std::span<int> values) { std::ranges::sort(values); }
struct Reused {
  const int value;
  explicit Reused(int x) noexcept : value(x) {}
  int dummy{0};
  ~Reused() noexcept { dummy = 1; }
};
static_assert(std::is_trivially_copyable_v<inplace_vector<std::uint64_t, 4096>>);
int main() {
  inplace_vector<int, 8> values;
  for (int x : std::array{7, 1, 9, 3})
    if (!values.try_push_back(x)) return 1;
  SortRange(std::span<int>{values});
  if (values.front() != 1 || values.back() != 9 || decltype(values)::capacity() != 8 || decltype(values)::max_size() != 8)
    return 2;
  const auto& constant = values;
  if (constant.cend() - constant.cbegin() != 4 || constant.data() != &constant[0]) return 3;
  inplace_vector<int, 8> comparison;
  comparison.try_push_back(2);
  if (!(values < comparison)) return 4;
  {
    inplace_vector<Tracked, 2> v;
    if (Tracked::live != 0) return 5;
    auto* first = v.try_emplace_back(3);
    auto* second = v.try_emplace_back(7);
    if (!first || !second || second - first != 1 || v.data() != first || Tracked::live != 2) return 6;
    auto before = Tracked::constructions;
    if (v.try_emplace_back(99) || Tracked::constructions != before) return 7;
    inplace_vector<Tracked, 2> copy(v);
    auto* moved_from = copy.data();
    inplace_vector<Tracked, 2> moved(std::move(copy));
    if (Tracked::live != 6 || moved[1].value != 7 || moved_from[0].value != -1) return 8;
    copy = v;
    auto& alias = copy;
    copy = alias;
    moved = std::move(copy);
    if (Tracked::live != 6 || moved[0].value != 3) return 9;
    moved.pop_back();
    moved.clear();
    moved.unchecked_emplace_back(4);
    if (Tracked::live != 5 || moved.front().value != 4) return 10;
  }
  if (Tracked::live != 0 || Tracked::constructions != Tracked::destructions) return 11;
  inplace_vector<Aligned, 2> aligned;
  auto* item = aligned.try_emplace_back(5);
  void* address = item;
  std::size_t space = sizeof(Aligned);
  if (!item || std::align(128, sizeof(Aligned), address, space) != static_cast<void*>(item)) return 12;
  inplace_vector<Tracked, 0> zero;
  if (decltype(zero)::size() != 0 || !decltype(zero)::empty() || !decltype(zero)::full() || zero.try_emplace_back(3) != nullptr)
    return 13;
  inplace_vector<Reused, 2> reuse;
  auto* empty_address = reuse.data();
  reuse.unchecked_emplace_back(17);
  reuse.clear();
  reuse.unchecked_emplace_back(29);
  const auto& const_reuse = reuse;
  if (reuse.data() != empty_address || const_reuse.front().value != 29) return 14;
  values.clear();
  if (!values.empty() || values.try_emplace_back(42) == nullptr || values.front() != 42) return 15;
  return 0;
}
