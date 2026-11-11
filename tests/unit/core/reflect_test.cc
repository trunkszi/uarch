#include <uarch/core/reflect.hpp>

struct Tag {
  int value;
};
struct Other {
  int value;
};
struct[[ = Other{9}, = Tag{7} ]] Single {
  [[maybe_unused]] unsigned first_;
  [[maybe_unused]] unsigned second_;
};
struct[[ = Tag{1}, = Tag{2} ]] Duplicate {};
static_assert(uarch::reflect::annotation_count_of<Tag>(^^Single) == 1);
static_assert(uarch::reflect::annotation_of<Tag>(^^Single)->value == 7);
static_assert(!uarch::reflect::annotation_of<int>(^^Single));
static_assert(uarch::reflect::annotation_count_of<Tag>(^^Duplicate) == 2);
static_assert(!uarch::reflect::annotation_of<Tag>(^^Duplicate));
constexpr auto kMembers =
    std::define_static_array(std::meta::nonstatic_data_members_of(^^Single, uarch::reflect::unchecked_context()));
static_assert(uarch::reflect::byte_offset(kMembers[1]) == sizeof(unsigned));
constexpr auto kText = uarch::reflect::freeze_string(std::string_view{"hello"}.substr(1, 3));
static_assert(kText == "ell");
constexpr auto kEmbedded = uarch::reflect::freeze_string(std::string_view{"a\0b", 3});
static_assert(kEmbedded.size() == 3 && kEmbedded[2] == 'b');
class Hidden {
  [[maybe_unused]] unsigned first_;
  [[maybe_unused]] unsigned second_;
};
constexpr auto kHidden =
    std::define_static_array(std::meta::nonstatic_data_members_of(^^Hidden, uarch::reflect::unchecked_context()));
static_assert(uarch::reflect::byte_offset(kHidden[1]) == sizeof(unsigned));
int main() { return kText != "ell"; }
