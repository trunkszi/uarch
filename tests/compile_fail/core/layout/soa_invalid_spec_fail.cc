// expected-error: SoaColumns requires a padding-free record|no matching.*function
#include <uarch/core/layout/soa_columns.hpp>
struct Padded {
  std::uint8_t a;
  std::uint64_t b;
};
struct Record {
  std::uint64_t field;
};
struct Other {
  std::uint64_t field;
};
int main() {
#if defined(UARCH_SOA_FOREIGN_CONTROL)
  uarch::layout::SoaColumns<Record, 4> columns;
  (void)columns.column<^^Other::field>();
#else
  uarch::layout::SoaColumns<Padded, 4> columns;
  (void)columns;
#endif
}
