// expected-error: noexcept callback requires a noexcept target and conversion
#include <uarch/core/flow/delegate.hpp>
struct Big {
  unsigned long words[4]{};
  Big() = default;
  Big(Big&&) noexcept(false) {}
};
struct Receiver {
  void consume(Big) noexcept {}
};
Receiver receiver;
auto invalid = uarch::flow::Callback<void(Big)>::bind<&Receiver::consume>(receiver);
