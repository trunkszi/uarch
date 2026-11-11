#include <limits>
#include <uarch/arch/entry.hpp>
#include <uarch/core/simd/vec.hpp>
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
#if defined(__x86_64__)
bool host_v3() {
  __builtin_cpu_init();
  unsigned a = 0, b = 0, c = 0, d = 0;
  constexpr unsigned required = (1U << 0) | (1U << 9) | (1U << 13) | (1U << 19) | (1U << 20) | (1U << 26) | (1U << 27);
  if (!__get_cpuid(1, &a, &b, &c, &d) || (c & required) != required) return false;
  if (!__get_cpuid(0x80000001, &a, &b, &c, &d) || (c & 1U) == 0) return false;
  return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2") &&
         __builtin_cpu_supports("fma") && __builtin_cpu_supports("f16c") && __builtin_cpu_supports("lzcnt") &&
         __builtin_cpu_supports("movbe") && __builtin_cpu_supports("popcnt");
}
bool host_v4() {
  return host_v3() && __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") &&
         __builtin_cpu_supports("avx512cd") && __builtin_cpu_supports("avx512dq") && __builtin_cpu_supports("avx512vl");
}
#endif

#if defined(__x86_64__)
[[gnu::target("arch=x86-64-v3"), gnu::always_inline]] inline std::uint64_t gather_function(X86V3 token, std::uint64_t v,
                                                                                           std::uint64_t m) noexcept {
  return token.bit_gather(v, m);
}
#endif
template <int N>
using WideVector = uarch::simd::vec<int, N>;
struct WideTransform {
  template <int N>
  [[gnu::always_inline]] WideVector<N> operator()(Scalar, WideVector<N> value) const noexcept {
    return value + WideVector<N>{10};
  }
#if defined(__x86_64__)
  template <int N>
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] WideVector<N> operator()(X86V3, WideVector<N> value) const noexcept {
    return value + WideVector<N>{10};
  }
  template <int N>
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] WideVector<N> operator()(X86V3SlowPdep,
                                                                                 WideVector<N> value) const noexcept {
    return value + WideVector<N>{10};
  }
  template <int N>
  [[gnu::target("arch=x86-64-v4"), gnu::always_inline]] WideVector<N> operator()(X86V4, WideVector<N> value) const noexcept {
    return value + WideVector<N>{10};
  }
#endif
#if defined(__aarch64__)
  template <int N>
  [[gnu::always_inline]] WideVector<N> operator()(Aarch64Neon, WideVector<N> value) const noexcept {
    return value + WideVector<N>{10};
  }
#endif
};
template <int N, Arch A>
bool wide_roundtrip(A token) {
  std::array<int, static_cast<std::size_t>(N)> input{};
  int seed = 1;
  asm volatile("" : "+r"(seed));
  for (int i = 0; i < N; ++i) input[static_cast<std::size_t>(i)] = seed + i;
  auto value = WideVector<N>::from_array(input);
  auto check = [&](const WideVector<N>& output) {
    auto lanes = output.to_array();
    for (int i = 0; i < N; ++i)
      if (lanes[static_cast<std::size_t>(i)] != seed + i + 10) return false;
    return true;
  };
  if (!check(run<WideTransform{}>(token, value)) || !check(dispatch_kernel<WideTransform{}>(Tier(token), value))) return false;
#if defined(__x86_64__)
  if constexpr (std::same_as<A, X86V3>) return check(run_x86_v3<WideTransform{}>(token, value));
  if constexpr (std::same_as<A, X86V3SlowPdep>) return check(run_x86_v3_slow_pdep<WideTransform{}>(token, value));
  if constexpr (std::same_as<A, X86V4>) return check(run_x86_v4<WideTransform{}>(token, value));
#endif
  return true;
}

#if defined(__x86_64__)
using V8 = WideVector<8>;
[[gnu::target("arch=x86-64-v3")]] int plain_cref(X86V3, const V8& value) noexcept { return value[3]; }
[[gnu::target("arch=x86-64-v3")]] int plain_byval(X86V3, V8 value) noexcept { return value[3]; }
[[gnu::target("arch=x86-64-v3")]] V8 plain_return(X86V3, V8 value) noexcept { return value + V8{10}; }
struct PlainDispatch {
  template <Arch A>
  int operator()(A, const V8&) const noexcept {
    std::abort();
  }
  [[gnu::target("arch=x86-64-v3")]] int operator()(X86V3 token, V8 value) const noexcept { return plain_byval(token, value); }
};
struct alignas(32) WideOwned {
  inline static int live = 0;
  int value;
  explicit WideOwned(int v) noexcept : value(v) { ++live; }
  WideOwned(const WideOwned& other) noexcept : value(other.value) { ++live; }
  WideOwned(WideOwned&& other) noexcept : value(other.value) { ++live; }
  ~WideOwned() noexcept { --live; }
};
struct OwnedTransform {
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] WideOwned operator()(X86V3, WideOwned value) const noexcept {
    value.value += 9;
    return value;
  }
};
struct MakeWide {
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] WideVector<8> operator()(X86V3, int seed) const noexcept {
    std::array<int, 8> values{};
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = seed + static_cast<int>(i);
    return WideVector<8>::from_array(values);
  }
};
struct SumWide {
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] int operator()(X86V3, WideVector<8> value) const noexcept {
    return uarch::simd::reduce(value);
  }
};
struct StoreWide {
  [[gnu::target("arch=x86-64-v3"), gnu::always_inline]] void operator()(X86V3, WideVector<8> value,
                                                                        std::span<int, 8> output) const noexcept {
    auto lanes = value.to_array();
    for (std::size_t i = 0; i < lanes.size(); ++i) output[i] = lanes[i];
  }
};
#endif

#if defined(__x86_64__)
[[gnu::target("arch=x86-64-v4"), gnu::noinline]] bool v4_scan_boundaries() {
  const auto token = X86V4::assume_supported(unchecked_tag);
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  const std::array<std::int64_t, 4> keys{kMax, kMin, 0, -1};
  const std::array<std::int64_t, 4> values{kMax, kMax, 1, 2};
  for (const auto target : std::array{kMin, std::int64_t{-1}, std::int64_t{0}, kMax}) {
    if (token.masked_sum_lt_i64x4(keys, values, target) != Scalar{}.masked_sum_lt_i64x4(keys, values, target) ||
        token.find_first_ge_i64x4(keys, target) != Scalar{}.find_first_ge_i64x4(keys, target))
      return false;
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    std::array<std::int64_t, 4> choices{};
    for (unsigned i = 0; i < 4; ++i) choices[i] = (mask & (1U << i)) != 0 ? 1 : -1;
    const auto found = token.find_first_ge_i64x4(choices, 0);
    if (found != Scalar{}.find_first_ge_i64x4(choices, 0)) return false;
    if (token.masked_sum_lt_i64x4(choices, values, 0) != Scalar{}.masked_sum_lt_i64x4(choices, values, 0)) return false;
  }
  return true;
}
#endif
int main() {
  if (run<Gather{}>(Scalar{}, 0xA0ULL, 0xF0ULL) != 10) return 1;
#if defined(__x86_64__)
  if (host_v3()) {
    auto token = X86V3::assume_supported(unchecked_tag);
    auto plain_input = V8::from_array({1, 2, 3, 4, 5, 6, 7, 8});
    if (run<plain_cref>(token, plain_input) != 4) return 30;
    if (run<plain_byval>(token, plain_input) != 4) return 31;
    if (run_x86_v3<plain_cref>(token, plain_input) != 4) return 32;
    if (dispatch_kernel<PlainDispatch{}>(Tier(token), plain_input) != 4) return 33;
    auto plain_result = run<plain_return>(token, plain_input).to_array();
    for (std::size_t i = 0; i < plain_result.size(); ++i)
      if (plain_result[i] != static_cast<int>(i) + 11) return 34;
    {
      WideOwned input(7);
      auto result = run<OwnedTransform{}>(token, input);
      if (result.value != 16 || WideOwned::live != 2) return 26;
    }
    if (WideOwned::live != 0) return 27;
    auto generated = run<MakeWide{}>(token, 1);
    if (run<SumWide{}>(token, generated) != 36) return 23;
    std::array<int, 8> stored{};
    run<StoreWide{}>(token, generated, std::span<int, 8>{stored});
    for (std::size_t i = 0; i < stored.size(); ++i)
      if (stored[i] != static_cast<int>(i) + 1) return 24;
    if (!wide_roundtrip<4>(token)) return 25;
    if (!wide_roundtrip<8>(X86V3::assume_supported(unchecked_tag))) return 20;
    if (!wide_roundtrip<16>(X86V3SlowPdep::assume_supported(unchecked_tag))) return 21;
    if (run<gather_function>(X86V3::assume_supported(unchecked_tag), 0xA0ULL, 0xF0ULL) != 10) return 1;
    if (run_x86_v3<Gather{}>(X86V3::assume_supported(unchecked_tag), 0xA0ULL, 0xF0ULL) != 10) return 1;
    if (dispatch_kernel<Gather{}>(Tier(X86V3SlowPdep::assume_supported(unchecked_tag)), 0xA0ULL, 0xF0ULL) != 10) return 1;
  }
  if (host_v4() && !v4_scan_boundaries()) return 35;
  if (host_v4() && !wide_roundtrip<16>(X86V4::assume_supported(unchecked_tag))) return 22;
  if (host_v4() && dispatch_kernel<Gather{}>(Tier(X86V4::assume_supported(unchecked_tag)), 0xA0ULL, 0xF0ULL) != 10) return 1;
#endif
#if defined(__aarch64__)
  if (run<Gather{}>(Aarch64Neon::assume_supported(unchecked_tag), 0xA0ULL, 0xF0ULL) != 10) return 1;
#endif
}
