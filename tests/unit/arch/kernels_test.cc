#include <array>
#include <bit>
#include <cstdio>
#include <uarch/arch/entry.hpp>
#if defined(__x86_64__)
#include <cpuid.h>
#endif
using namespace uarch::arch;
struct Gather {
  [[gnu::always_inline]] std::uint64_t operator()(Scalar a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3SlowPdep a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::uint64_t operator()(X86V4 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] std::uint64_t operator()(Aarch64Neon a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_gather(v, m);
  }
#endif
};
struct Scatter {
  [[gnu::always_inline]] std::uint64_t operator()(Scalar a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_scatter(v, m);
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_scatter(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::uint64_t operator()(X86V3SlowPdep a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_scatter(v, m);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::uint64_t operator()(X86V4 a, std::uint64_t v,
                                                                                 std::uint64_t m) const noexcept {
    return a.bit_scatter(v, m);
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] std::uint64_t operator()(Aarch64Neon a, std::uint64_t v, std::uint64_t m) const noexcept {
    return a.bit_scatter(v, m);
  }
#endif
};
struct Sum {
  [[gnu::always_inline]] std::int64_t operator()(Scalar a, std::span<const std::int64_t, 4> k,
                                                 std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    return a.masked_sum_lt_i64x4(k, v, limit);
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] std::int64_t operator()(X86V3 a, std::span<const std::int64_t, 4> k,
                                                                                std::span<const std::int64_t, 4> v,
                                                                                std::int64_t limit) const noexcept {
    return a.masked_sum_lt_i64x4(k, v, limit);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"),
    gnu::always_inline]] std::int64_t operator()(X86V3SlowPdep a, std::span<const std::int64_t, 4> k,
                                                 std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    return a.masked_sum_lt_i64x4(k, v, limit);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] std::int64_t operator()(X86V4 a, std::span<const std::int64_t, 4> k,
                                                                                std::span<const std::int64_t, 4> v,
                                                                                std::int64_t limit) const noexcept {
    return a.masked_sum_lt_i64x4(k, v, limit);
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] std::int64_t operator()(Aarch64Neon a, std::span<const std::int64_t, 4> k,
                                                 std::span<const std::int64_t, 4> v, std::int64_t limit) const noexcept {
    return a.masked_sum_lt_i64x4(k, v, limit);
  }
#endif
};
struct Find {
  [[gnu::always_inline]] std::optional<std::uint32_t> operator()(Scalar a, std::span<const std::int64_t, 4> k,
                                                                 std::int64_t target) const noexcept {
    return a.find_first_ge_i64x4(k, target);
  }
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"),
    gnu::always_inline]] std::optional<std::uint32_t> operator()(X86V3 a, std::span<const std::int64_t, 4> k,
                                                                 std::int64_t target) const noexcept {
    return a.find_first_ge_i64x4(k, target);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v3"),
    gnu::always_inline]] std::optional<std::uint32_t> operator()(X86V3SlowPdep a, std::span<const std::int64_t, 4> k,
                                                                 std::int64_t target) const noexcept {
    return a.find_first_ge_i64x4(k, target);
  }
#endif
#if defined(__x86_64__)
  [[gnu::target("arch=x86-64-v4"),
    gnu::always_inline]] std::optional<std::uint32_t> operator()(X86V4 a, std::span<const std::int64_t, 4> k,
                                                                 std::int64_t target) const noexcept {
    return a.find_first_ge_i64x4(k, target);
  }
#endif
#if defined(__aarch64__)
  [[gnu::always_inline]] std::optional<std::uint32_t> operator()(Aarch64Neon a, std::span<const std::int64_t, 4> k,
                                                                 std::int64_t target) const noexcept {
    return a.find_first_ge_i64x4(k, target);
  }
#endif
};

#if defined(__x86_64__)
bool host_v3() {
  __builtin_cpu_init();
  unsigned a = 0, b = 0, c = 0, d = 0;
  constexpr unsigned required = (1U << 0) | (1U << 9) | (1U << 13) | (1U << 19) | (1U << 20) | (1U << 26) | (1U << 27);
  if (!__get_cpuid(1, &a, &b, &c, &d) || (c & required) != required) return false;
  return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2") &&
         __builtin_cpu_supports("fma") && __builtin_cpu_supports("f16c") && __builtin_cpu_supports("lzcnt") &&
         __builtin_cpu_supports("movbe") && __builtin_cpu_supports("popcnt");
}
bool host_v4() {
  return host_v3() && __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") &&
         __builtin_cpu_supports("avx512cd") && __builtin_cpu_supports("avx512dq") && __builtin_cpu_supports("avx512vl");
}
#endif

std::uint64_t next(std::uint64_t& state) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}
template <Arch A>
bool compare(A token, std::uint64_t v, std::uint64_t m, const std::array<std::int64_t, 4>& k,
             const std::array<std::int64_t, 4>& vals, std::int64_t limit) {
  Scalar scalar;
  return run<Gather{}>(token, v, m) == scalar.bit_gather(v, m) && run<Scatter{}>(token, v, m) == scalar.bit_scatter(v, m) &&
         run<Sum{}>(token, std::span<const std::int64_t, 4>(k), std::span<const std::int64_t, 4>(vals), limit) ==
             scalar.masked_sum_lt_i64x4(k, vals, limit) &&
         run<Find{}>(token, std::span<const std::int64_t, 4>(k), limit) == scalar.find_first_ge_i64x4(k, limit);
}
int main() {
  std::uint64_t state = 0x123456789abcdefULL;
  for (unsigned i = 0; i < 4096; ++i) {
    auto v = next(state), m = next(state);
    if (i < 64) m = std::uint64_t{1} << i;
    if (i == 64) m = 0;
    if (i == 65) m = UINT64_MAX;
    std::array<std::int64_t, 4> k{}, vals{};
    for (unsigned j = 0; j < 4; ++j) {
      k[j] = std::bit_cast<std::int64_t>(next(state));
      vals[j] = std::bit_cast<std::int64_t>(next(state));
    }
    auto limit = i % 2 == 0 ? k[i % 4] : std::bit_cast<std::int64_t>(next(state));
    std::uint64_t packed = 0;
    unsigned bit = 0;
    for (unsigned j = 0; j < 64; ++j)
      if ((m & (std::uint64_t{1} << j)) != 0) {
        packed |= ((v >> j) & 1) << bit;
        ++bit;
      }
    if (Scalar{}.bit_gather(v, m) != packed || Scalar{}.bit_scatter(packed, m) != (v & m)) return 1;
    std::uint64_t sum = 0;
    std::optional<std::uint32_t> found;
    for (std::uint32_t j = 0; j < 4; ++j) {
      if (k[j] < limit) sum += static_cast<std::uint64_t>(vals[j]);
      if (!found && k[j] >= limit) found = j;
    }
    if (Scalar{}.masked_sum_lt_i64x4(k, vals, limit) != std::bit_cast<std::int64_t>(sum) ||
        Scalar{}.find_first_ge_i64x4(k, limit) != found)
      return 1;
#if defined(__x86_64__)
    if (host_v3() && (!compare(X86V3::assume_supported(unchecked_tag), v, m, k, vals, limit) ||
                      !compare(X86V3SlowPdep::assume_supported(unchecked_tag), v, m, k, vals, limit)))
      return 1;
    if (host_v4() && !compare(X86V4::assume_supported(unchecked_tag), v, m, k, vals, limit)) return 1;
#endif
#if defined(__aarch64__)
    if (!compare(Aarch64Neon::assume_supported(unchecked_tag), v, m, k, vals, limit)) return 1;
#endif
  }
#if defined(__x86_64__)
  std::printf("4096 cases: v3=%d v4=%d (unsupported tiers skipped)\n", host_v3(), host_v4());
#endif
}
