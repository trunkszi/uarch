// expected-error: (no viable conversion|conversion from|could not convert).*Delegate
#include <uarch/primitives.hpp>
struct Receiver {
  int method(int value) { return value; }
};
Receiver receiver;
uarch::Callback<int(int)> invalid = uarch::bind<&Receiver::method>(receiver);
