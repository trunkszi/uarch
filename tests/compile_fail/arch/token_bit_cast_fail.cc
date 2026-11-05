// expected-error: no matching function
#include <bit>
#include <uarch/arch/token.hpp>
int main() { return std::bit_cast<uarch::arch::X86V3>(std::array<std::byte, sizeof(uarch::arch::X86V3)>{}).fast_pdep; }
