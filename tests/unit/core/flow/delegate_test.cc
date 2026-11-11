#include <array>
#include <expected>
#include <optional>
#include <span>
#include <uarch/core/flow/delegate.hpp>
#include <variant>
using uarch::flow::Callback;
namespace {
struct Receiver {
  int total{0};
  constexpr void add(int x) noexcept { total += x; }
  int& reference() noexcept { return total; }
  int boolean(bool value) noexcept { return value ? 1 : -1; }
  int optional(std::optional<int> value) noexcept { return value.value_or(-1); }
  int expected(std::expected<int, int> value) noexcept { return value ? *value : -1; }
  int variant(std::variant<int, long> value) noexcept {
    if (const auto* number = std::get_if<int>(&value)) return *number;
    return static_cast<int>(*std::get_if<long>(&value));
  }
  void batch(std::span<const int> values) noexcept {
    for (int value : values) total += value;
  }
};
int raw(void* context, int value) noexcept { return *static_cast<int*>(context) + value; }
struct SelfReset {
  Callback<int(int)> callback;
  int calls{0};
  int invoke(int x) noexcept {
    callback.reset();
    ++calls;
    return x;
  }
};
extern "C" int c_first(void* p, int value) noexcept { return raw(p, value); }
extern "C" int c_last(int value, void* p) noexcept { return raw(p, value); }
}  // namespace
int main() {
  Receiver receiver;
  auto boolean = Callback<int(bool)>::bind<&Receiver::boolean>(receiver);
  auto optional = Callback<int(std::optional<int>)>::bind<&Receiver::optional>(receiver);
  auto expected = Callback<int(std::expected<int, int>)>::bind<&Receiver::expected>(receiver);
  auto variant = Callback<int(std::variant<int, long>)>::bind<&Receiver::variant>(receiver);
  if (boolean(true) != 1 || boolean(false) != -1 || optional(std::optional{7}) != 7 || optional(std::nullopt) != -1 ||
      expected(std::expected<int, int>{9}) != 9 || expected(std::expected<int, int>{std::unexpected(3)}) != -1 ||
      variant(std::variant<int, long>{11L}) != 11 || variant(std::variant<int, long>{13}) != 13)
    return 7;
  const auto add = Callback<void(int)>::bind<&Receiver::add>(receiver);
  add(3);
  const auto reference = Callback<int&()>::bind<&Receiver::reference>(receiver);
  if (&reference() != &receiver.total) return 1;
  reference() = 7;
  const std::array values{1, 2, 3};
  Callback<void(std::span<const int>)>::bind<&Receiver::batch>(receiver)(values);
  if (receiver.total != 13) return 2;
  int offset = 5;
  auto callback = Callback<int(int)>::from_raw(&raw, &offset);
  if (callback(4) != 9 || c_first(callback.context(), 4) != 9 || c_last(4, callback.context()) != 9) return 3;
  auto copied = callback;
  if (copied != callback) return 4;
  copied.reset();
  if (copied || !callback || callback.stub()(callback.context(), 2) != 7) return 5;
  SelfReset self;
  self.callback = Callback<int(int)>::bind<&SelfReset::invoke>(self);
  auto snapshot = self.callback;
  if (snapshot(11) != 11 || self.callback || self.calls != 1) return 6;
  return 0;
}
