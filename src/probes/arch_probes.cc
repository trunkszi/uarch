#include <uarch/arch/token.hpp>
extern "C" [[gnu::noinline]] std::uint64_t uarch_arch_scalar_gather(std::uint64_t v, std::uint64_t m) {
  return uarch::arch::Scalar{}.bit_gather(v, m);
}
#if defined(__x86_64__)
extern "C"
    [[gnu::target("arch=x86-64-v4"), gnu::noinline]] std::uint64_t uarch_arch_v4_gather(std::uint64_t v, std::uint64_t m) {
  return uarch::arch::X86V4::assume_supported(uarch::arch::unchecked_tag).bit_gather(v, m);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint64_t uarch_arch_v3_scatter(std::uint64_t v,
                                                                                                std::uint64_t m) {
  return uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag).bit_scatter(v, m);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint64_t x86_v3_gather(std::uint64_t v, std::uint64_t m) {
  return uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag).bit_gather(v, m);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint64_t x86_v3_slow_pdep_gather(std::uint64_t v,
                                                                                                  std::uint64_t m) {
  return uarch::arch::X86V3SlowPdep::assume_supported(uarch::arch::unchecked_tag).bit_gather(v, m);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint64_t x86_v3_slow_pdep_scatter(std::uint64_t v,
                                                                                                   std::uint64_t m) {
  return uarch::arch::X86V3SlowPdep::assume_supported(uarch::arch::unchecked_tag).bit_scatter(v, m);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::int64_t uarch_arch_masked_sum(const std::int64_t* k,
                                                                                               const std::int64_t* v,
                                                                                               std::int64_t limit) {
  return uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag)
      .masked_sum_lt_i64x4(std::span<const std::int64_t, 4>(k, 4), std::span<const std::int64_t, 4>(v, 4), limit);
}
extern "C" [[gnu::target("arch=x86-64-v3"), gnu::noinline]] std::uint32_t uarch_arch_first_ge(const std::int64_t* k,
                                                                                              std::int64_t target) {
  auto found = uarch::arch::X86V3::assume_supported(uarch::arch::unchecked_tag)
                   .find_first_ge_i64x4(std::span<const std::int64_t, 4>(k, 4), target);
  return found ? *found : UINT32_MAX;
}

extern "C" [[gnu::target("arch=x86-64-v4"), gnu::noinline]] std::int64_t uarch_arch_v4_masked_sum(const std::int64_t* keys,
                                                                                                  const std::int64_t* values,
                                                                                                  std::int64_t limit) {
  return uarch::arch::X86V4::assume_supported(uarch::arch::unchecked_tag)
      .masked_sum_lt_i64x4(std::span<const std::int64_t, 4>(keys, 4), std::span<const std::int64_t, 4>(values, 4), limit);
}
extern "C" [[gnu::target("arch=x86-64-v4"), gnu::noinline]] std::uint32_t uarch_arch_v4_first_ge(const std::int64_t* keys,
                                                                                                 std::int64_t target) {
  auto found = uarch::arch::X86V4::assume_supported(uarch::arch::unchecked_tag)
                   .find_first_ge_i64x4(std::span<const std::int64_t, 4>(keys, 4), target);
  return found ? *found : UINT32_MAX;
}
#endif
