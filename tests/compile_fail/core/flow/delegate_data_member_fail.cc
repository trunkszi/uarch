// expected-error: member data pointers are not callbacks; bind a member function
#include <uarch/core/flow/delegate.hpp>
struct Receiver {
  int field;
};
static_assert(std::is_nothrow_invocable_r_v<int, int Receiver::*, Receiver&>);
Receiver receiver;
#if defined(UARCH_STATIC_DATA_MEMBER)
auto invalid = uarch::flow::Callback<int()>::bind<&Receiver::field>(receiver);
#else
auto invalid = uarch::flow::bind<&Receiver::field>(receiver);
#endif
