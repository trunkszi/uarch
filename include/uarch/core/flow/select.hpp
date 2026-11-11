#pragma once
#include <uarch/core/flow/predicate.hpp>
namespace uarch::flow {
/// Both arms are evaluated; selection does not guard potentially invalid accesses.
template <RegisterSelectable T>
constexpr T select_register(Predicate cond, T a, T b) noexcept {
  if consteval {
    return cond.as_bool() ? a : b;
  } else {
    const auto c = static_cast<unsigned>(cond.as_bool());
#if defined(__x86_64__)
    if constexpr (sizeof(T) == 4)
      asm("test %k[c], %k[c]\n\tcmovne %k[a], %k[b]" : [b] "+r"(b) : [a] "r"(a), [c] "r"(c) : "cc");
    else
      asm("test %k[c], %k[c]\n\tcmovne %q[a], %q[b]" : [b] "+r"(b) : [a] "r"(a), [c] "r"(c) : "cc");
#elif defined(__aarch64__)
    if constexpr (sizeof(T) == 4)
      asm("cmp %w[c], #0\n\tcsel %w[b], %w[a], %w[b], ne" : [b] "+r"(b) : [a] "r"(a), [c] "r"(c) : "cc");
    else
      asm("cmp %w[c], #0\n\tcsel %x[b], %x[a], %x[b], ne" : [b] "+r"(b) : [a] "r"(a), [c] "r"(c) : "cc");
#else
    b = cond.as_bool() ? a : b;
#endif
    return b;
  }
}
template <class T>
constexpr T select(Predicate cond, T a, T b) {
#if defined(__clang__)
  if constexpr (std::is_integral_v<T>) return __builtin_unpredictable(cond.as_bool()) ? std::move(a) : std::move(b);
#endif
  return cond.as_bool() ? std::move(a) : std::move(b);
}
}  // namespace uarch::flow
