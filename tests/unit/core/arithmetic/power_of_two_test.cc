#include <cstdlib>
#include <uarch/core/arithmetic/power_of_two.hpp>
#include <utility>

using uarch::arithmetic::PowerOfTwo;
template <unsigned Log>
void check() {
  using Power = PowerOfTwo<Log>;
  for (const auto n : {std::uint64_t{0}, std::uint64_t{1}, Power::value - 1, Power::value, UINT64_MAX}) {
    if (Power::div(n) != n / Power::value || Power::rem(n) != n % Power::value ||
        Power::is_multiple_of(n) != (n % Power::value == 0) || Power::align_down(n) != (n / Power::value) * Power::value)
      std::abort();
    const __uint128_t rounded = ((static_cast<__uint128_t>(n) + Power::value - 1) / Power::value) * Power::value;
    const auto result = Power::checked_align_up(n);
    if (rounded > UINT64_MAX) {
      if (result) std::abort();
    } else if (!result || *result != rounded)
      std::abort();
    if (Power::alignment_offset(n) != (rounded - n)) std::abort();
  }
}
template <std::size_t... Index>
void all(std::index_sequence<Index...>) {
  (check<static_cast<unsigned>(Index)>(), ...);
}
int main() { all(std::make_index_sequence<64>{}); }
