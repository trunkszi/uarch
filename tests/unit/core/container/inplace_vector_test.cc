#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <ranges>
#include <span>
#include <uarch/uarch.hpp>

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
  Tracked& operator=(const Tracked&) noexcept = default;
  Tracked& operator=(Tracked&&) noexcept = default;
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
void ExtendedRuntime();
void BoundaryRuntime();
int main() {
  {
    inplace_vector<Tracked, 4> resized;
    resized.emplace_back(19);
    if (!resized.try_resize(4, resized.front()) || resized.back().value != 19 || Tracked::live != 4) return 30;
    const auto before = Tracked::constructions;
    if (resized.try_resize(5, resized.front()) || Tracked::constructions != before) return 31;
    if (!resized.try_resize(1, resized.front()) || Tracked::live != 1) return 32;
    if (!resized.try_resize(0, resized.front()) || Tracked::live != 0) return 33;
  }
  ExtendedRuntime();
  BoundaryRuntime();
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
  if (!zero.empty() || !zero.full() || zero.try_emplace_back(3) != nullptr) return 13;
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

static_assert(sizeof(inplace_vector<int, 4>) == sizeof(int) * 4 + alignof(int));
consteval bool ExtendedApi() {
  inplace_vector<int, 8> values{1, 3};
  values.insert(values.begin() + 1, 2);
  values.resize(5, 4);
  values.erase(values.begin() + 3, values.end());
  const std::array tail{5, 6};
  values.append_range(tail);
  return values.size() == 5 && values.at(1) == 2 && *values.rbegin() == 6;
}
static_assert(ExtendedApi());

struct NonTrivialCopy {
  static inline int copies = 0;
  int value;
  explicit NonTrivialCopy(int x = 0) noexcept : value(x) {}
  NonTrivialCopy(const NonTrivialCopy& other) noexcept : value(other.value) { ++copies; }
  NonTrivialCopy(NonTrivialCopy&&) noexcept = default;
  NonTrivialCopy& operator=(const NonTrivialCopy&) noexcept = default;
  NonTrivialCopy& operator=(NonTrivialCopy&&) noexcept = default;
};
struct Immobile {
  Immobile() = delete;
  Immobile(const Immobile&) = delete;
  Immobile(Immobile&&) = delete;
  ~Immobile() { std::abort(); }
};
static_assert(!std::is_trivially_copy_constructible_v<inplace_vector<NonTrivialCopy, 4>>);
static_assert(std::is_trivially_move_constructible_v<inplace_vector<NonTrivialCopy, 4>>);
static_assert(!std::is_trivially_copy_assignable_v<inplace_vector<NonTrivialCopy, 4>>);
static_assert(std::is_trivially_move_assignable_v<inplace_vector<NonTrivialCopy, 4>>);
static_assert(std::is_trivially_destructible_v<inplace_vector<NonTrivialCopy, 4>>);
static_assert(std::is_empty_v<inplace_vector<Immobile, 0>>);
static_assert(__is_trivial(inplace_vector<Immobile, 0>));
static_assert(std::is_same_v<uarch::container::detail::smallest_size_t<255>, std::uint8_t>);
static_assert(std::is_same_v<uarch::container::detail::smallest_size_t<256>, std::uint16_t>);
static_assert(std::is_same_v<uarch::container::detail::smallest_size_t<65536>, std::uint32_t>);
static_assert(std::is_same_v<uarch::container::detail::smallest_size_t<UINT64_C(4294967296)>, std::size_t>);

consteval bool ConstructorsAndRanges() {
  inplace_vector<int, 12> zeroes(2);
  if (zeroes[0] != 0 || zeroes[1] != 0) return false;
  inplace_vector<int, 12> filled(2, 7);
  if (filled.back() != 7) return false;
  std::array input{1, 2, 3};
  inplace_vector<int, 12> pairs(input.begin(), input.end());
  inplace_vector<int, 12> ranged(std::from_range, input | std::views::take(2));
  if (pairs.size() != 3 || ranged.size() != 2) return false;
  pairs = {4, 5};
  pairs.assign(3, pairs.front());
  if (pairs.back() != 4) return false;
  pairs.assign(input.begin(), input.end());
  pairs.assign_range(input | std::views::drop(1));
  pairs.assign({7, 8});
  pairs.resize(3);
  pairs.resize(4, 9);
  decltype(pairs)::reserve(12);
  decltype(pairs)::shrink_to_fit();
  const auto& constant = pairs;
  if (pairs.at(2) != 0 || constant.at(3) != 9 || *constant.crbegin() != 9) return false;
  if (pairs.rend() - pairs.rbegin() != 4 || constant.crend() - constant.crbegin() != 4) return false;
  pairs.insert(pairs.begin() + 1, 2, pairs.front());
  pairs.insert(pairs.end(), input.begin(), input.end());
  pairs.insert(pairs.begin(), {5, 6});
  pairs.erase(pairs.begin(), pairs.begin() + 2);
  pairs.erase(pairs.begin());
  pairs.emplace(pairs.begin(), 42);
  pairs.insert_range(pairs.end(), input | std::views::take(1));
  if (pairs.front() != 42 || pairs.back() != 1) return false;
  pairs.clear();
  pairs.push_back(1);
  int two = 2;
  pairs.push_back(two);
  pairs.emplace_back(3);
  if (uarch::erase(pairs, 2) != 1) return false;
  if (uarch::erase_if(pairs, [](int x) { return x == 1; }) != 1 || pairs.front() != 3) return false;
  inplace_vector<int, 12> other{5, 6};
  swap(pairs, other);
  pairs.swap(pairs);
  if (pairs.size() != 2 || other.size() != 1 || other.front() != 3) return false;
  const auto& const_pairs = pairs;
  return *pairs.rbegin() == 6 && *const_pairs.rend().base() == 5 && pairs.cbegin() == pairs.begin();
}
static_assert(ConstructorsAndRanges());
consteval bool PartialAppendAndEmpty() {
  inplace_vector<int, 3> values{9};
  std::array input{1, 2, 3};
  auto stop = values.try_append_range(input);
  if (stop != input.begin() + 2 || values.size() != 3 || values.back() != 2) return false;
  if (values.try_append_range(input) != input.begin()) return false;
  static_assert(std::is_same_v<decltype(values.try_append_range(std::array{1})), std::ranges::dangling>);
  inplace_vector<Immobile, 0> empty, other;
  empty.swap(other);
  empty.clear();
  empty.erase(empty.begin(), empty.end());
  if (uarch::erase_if(empty, [](const Immobile&) { return true; }) != 0) return false;
  return empty == other && (empty <=> other) == 0 && empty.rbegin() == empty.rend();
}
static_assert(PartialAppendAndEmpty());
consteval bool FreestandingAndFacade() {
  uarch::container::freestanding::inplace_vector<int, 4> values;
  values.try_push_back(3);
  values.unchecked_push_back(4);
  auto copy(values);
  copy = values;
  copy.erase(copy.begin());
  uarch::prelude::inplace_vector<int, 4> exported{7, 8};
  return copy.front() == 4 && uarch::prelude::erase(exported, 7) == 1 && exported.front() == 8;
}
static_assert(FreestandingAndFacade());

static void Check(bool condition) {
  if (!condition) std::abort();
}
void ExtendedRuntime() {
  {
    inplace_vector<Tracked, 16> values;
    values.emplace_back(1);
    values.emplace_back(3);
    auto it = values.emplace(values.begin() + 1, 2);
    Check(it->value == 2 && values[2].value == 3);
    values.insert(values.begin(), values.back());
    values.insert(values.end(), Tracked{4});
    values.insert(values.begin() + 1, 2, values.front());
    std::array input{Tracked{7}, Tracked{8}};
    values.insert(values.begin() + 2, input.begin(), input.end());
    values.insert_range(values.end(), input | std::views::take(1));
    Check(values.size() == 10 && values.back().value == 7);
    values.erase(values.begin() + 1, values.begin() + 4);
    Check(values.size() == 7 && values[1].value == 3);
    Check(uarch::erase_if(values, [](const Tracked& value) { return value.value == 3; }) == 3);
    values.resize(6, Tracked{9});
    Check(values.size() == 6 && values.back().value == 9);
    values.assign(3, values.back());
    Check(values.size() == 3 && values.front().value == 9);
    values.assign(input.begin(), input.end());
    values.assign_range(input | std::views::drop(1));
    values.assign({Tracked{5}, Tracked{6}});
    inplace_vector<Tracked, 16> other;
    other.emplace_back(10);
    swap(values, other);
    Check(values.size() == 1 && values.front().value == 10 && other.size() == 2 && other.back().value == 6);
    other.swap(values);
    Check(values.size() == 2 && values[0].value == 5 && other.front().value == 10);
    values.swap(values);
    values.clear();
    Check(values.empty());
  }
  Check(Tracked::live == 0 && Tracked::constructions == Tracked::destructions);
  {
    inplace_vector<std::unique_ptr<int>, 4> values;
    values.emplace_back(std::make_unique<int>(1));
    values.emplace(values.begin(), std::make_unique<int>(2));
    inplace_vector<std::unique_ptr<int>, 4> other(std::move(values));
    // The sequence move constructor retains its source size with moved-from elements.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    Check(other.size() == 2 && *other.front() == 2 && values.front() == nullptr);
    values = std::move(other);
    values.erase(values.begin());
    Check(values.size() == 1 && *values.front() == 1);
  }
  {
    inplace_vector<NonTrivialCopy, 4> values;
    values.emplace_back(17);
    const auto before = NonTrivialCopy::copies;
    auto copy = values;
    Check(copy.front().value == 17 && NonTrivialCopy::copies == before + 1);
    copy = values;
    auto moved = std::move(copy);
    Check(moved.front().value == 17);
    copy = std::move(moved);
    Check(copy.front().value == 17);
  }
  {
    inplace_vector<int, 3> values;
    std::array input{1, 2, 3, 4};
    auto range = std::ranges::subrange(std::counted_iterator(input.begin(), 4), std::default_sentinel);
    auto first_unconsumed = values.try_append_range(range);
    Check(first_unconsumed.count() == 1 && *first_unconsumed == 4 && values.full());
    inplace_vector<int, 8> target(std::from_range, range);
    target.insert_range(target.begin() + 1, input | std::views::take_while([](int value) { return value < 3; }));
    Check(target.size() == 6 && target[1] == 1 && target[2] == 2 && target.back() == 4);
    target.insert(target.begin(), target.back());
    Check(target.front() == 4 && target.back() == 4);
  }
}
template <class F>
static void CheckAbort(F operation) {
  const auto child = ::fork();
  Check(child >= 0);
  if (child == 0) {
    const rlimit limit{0, 0};
    if (::setrlimit(RLIMIT_CORE, &limit) != 0) std::_Exit(2);
    operation();
    std::_Exit(0);
  }
  int status{};
  Check(::waitpid(child, &status, 0) == child);
  Check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
void BoundaryRuntime() {
  CheckAbort([] {
    inplace_vector<int, 0> value;
    value.push_back(1);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value{1};
    value.emplace_back(2);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.append_range(std::array{1, 2});
  });
  CheckAbort([] {
    inplace_vector<int, 1> value{1};
    value.insert(value.begin(), 2);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.insert(value.begin(), 2, 1);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.insert_range(value.begin(), std::array{1, 2});
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.assign(2, 1);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.assign_range(std::array{1, 2});
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.resize(2);
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    decltype(value)::reserve(2);
  });
  CheckAbort([] { inplace_vector<int, 1> value(2); });
  CheckAbort([] { inplace_vector<int, 1> value{1, 2}; });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    (void)value.at(0);
  });
  CheckAbort([] {
    const inplace_vector<int, 1> value{1};
    (void)value[1];
  });
  CheckAbort([] {
    inplace_vector<int, 0> value;
    (void)value.front();
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    (void)value.back();
  });
  CheckAbort([] {
    inplace_vector<int, 1> value;
    value.pop_back();
  });
}
static_assert(std::is_same_v<decltype(std::declval<inplace_vector<int, 4>&>().push_back(1)), int&>);
struct NsdmiSegment {
  int kind{1};
  int precision{6};
};
consteval bool NsdmiStorage() {
  inplace_vector<NsdmiSegment, 8> segments;
  segments.emplace_back();
  segments.push_back(NsdmiSegment{2, 9});
  auto copy = segments;
  copy.erase(copy.begin());
  return segments.front().kind == 1 && segments.front().precision == 6 && copy.front().kind == 2;
}
static_assert(NsdmiStorage());
static_assert(std::is_trivially_copyable_v<inplace_vector<NsdmiSegment, 8>>);
struct ExplicitDefault {
  int value;
  explicit constexpr ExplicitDefault() noexcept : value(21) {}
};
static_assert([] consteval {
  inplace_vector<ExplicitDefault, 2> values;
  values.emplace_back();
  return values.front().value == 21;
}());

static_assert([] consteval {
  inplace_vector<int, 4> values;
  if (!values.try_resize(2) || values[0] != 0) return false;
  values[0] = 7;
  if (!values.try_resize(4, values[0]) || values[3] != 7) return false;
  if (values.try_resize(5) || values.size() != 4) return false;
  if (!values.try_resize(1, values[0]) || values.size() != 1) return false;
  uarch::container::freestanding::inplace_vector<int, 0> empty;
  return empty.try_resize(0) && !empty.try_resize(1);
}());
