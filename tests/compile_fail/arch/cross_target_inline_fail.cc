// expected-error: target feature|target specific option mismatch
#include <uarch/arch/token.hpp>
#if defined(__x86_64__)
std::uint64_t gather(std::uint64_t v, std::uint64_t m) {
  return uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag).bit_gather(v, m);
}
int main(int argc, char**) { return static_cast<int>(gather(static_cast<std::uint64_t>(argc), 0xf0)); }
#else
static_assert(false, "target feature boundary requires an x86 compiler");
#endif
