#include <limits>
#include <meta>
#include <string_view>
#include <type_traits>

struct Note {
  int value;
};
struct[[= Note{7}]] Record {
  int first;
  int second;
};
constexpr auto kMembers =
    std::define_static_array(std::meta::nonstatic_data_members_of(^^Record, std::meta::access_context::current()));
constexpr auto kNotes = std::define_static_array(std::meta::annotations_of(^^Record));
static_assert(std::meta::extract<Note>(std::meta::constant_of(kNotes[0])).value == 7);
using Reflected = [:^^Record:];
static_assert(std::is_same_v<Record, Reflected>);
static_assert(std::define_static_string("reflection")[0] == 'r');

consteval int CountMembers() {
  int count = 0;
  template for (constexpr auto kItem : kMembers) { count += std::meta::is_nonstatic_data_member(kItem) ? 1 : 0; }
  return count;
}
template <class T>
consteval int CountMembersOf() {
  int count = 0;
  template for (constexpr auto kMember :
                std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()))) {
    count += std::meta::is_nonstatic_data_member(kMember) ? 1 : 0;
  }
  return count;
}
template <class... Types>
using First = Types...[0];
struct Restricted {
  void Invoke() = delete ("smoke deleted operation");
};
static_assert(CountMembers() == 2 && CountMembersOf<Record>() == 2, std::string_view("expansion mismatch"));
static_assert(std::is_same_v<First<int, double>, int>);
int main() {
#if defined(UARCH_UBSAN_FAILURE_CONTROL)
  volatile int input = std::numeric_limits<int>::max();
  return input + 1;
#else
  return CountMembers() != 2;
#endif
}
