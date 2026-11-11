#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <uarch/core/layout/register_passable.hpp>
namespace uarch::probes {
struct RegisterPair {
  std::uint64_t a, b;
};
struct RegisterSmall {
  std::uint32_t value;
};
struct RegisterCAbi {
  std::uint32_t a, b;
};
struct NonTrivialDtor8 {
  std::uint64_t value;
  ~NonTrivialDtor8() noexcept { value = 0; }
};
struct HfaPair {
  double a, b;
};
static_assert(layout::RegisterPassable<RegisterPair> && layout::GprPair<RegisterPair>);
static_assert(layout::RegisterPassable<RegisterSmall> && layout::RegisterPassable<RegisterCAbi>);
static_assert(layout::aapcs64_classify<RegisterPair>() ==
              layout::AbiShape{layout::AbiClass::Integer, layout::AbiClass::Integer, 2});
static_assert(layout::aapcs64_classify<HfaPair>() == layout::AbiShape{layout::AbiClass::Sse, layout::AbiClass::Sse, 2});
enum class AbiRule { None, Registers, MemoryControl, GprCallback };
enum Instruction : std::uint32_t {
  Jcc = 1U << 0,
  Movabs = 1U << 1,
  Call = 1U << 2,
  Tzcnt = 1U << 3,
  Blsr = 1U << 4,
  Cmov = 1U << 5,
  Div = 1U << 6,
  Idiv = 1U << 7,
  Udivti3 = 1U << 8,
  Pext = 1U << 9,
  Pdep = 1U << 10,
  Prefetchw = 1U << 11,
  PrefetchT0 = 1U << 12,
  Stack = 1U << 13,
  Simd = 1U << 14,
  Compare = 1U << 15,
  DirectCall = 1U << 16,
  TailJmp = 1U << 17
};
struct Gate {
  std::string_view symbol{};
  std::uint32_t require{0}, deny{0};
  std::size_t max_instructions{0};
  AbiRule abi{AbiRule::None};
  std::string_view section{};
  double max_throughput{0};
  bool pointer_args{false};
  std::string_view mca_cpu{"znver4"};
};
inline constexpr std::array gates{
    Gate{"delegate_identity", 0, DirectCall | Stack | Div | Idiv | Jcc, 3, AbiRule::GprCallback, {}, 0.7, false, "znver5"},
    Gate{"delegate_invoke_i32",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         3,
         AbiRule::GprCallback,
         {},
         0.7,
         false,
         "znver5"},
    Gate{"delegate_invoke_span",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         4,
         AbiRule::GprCallback,
         {},
         0.9,
         false,
         "znver5"},
    Gate{"delegate_invoke_member",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         4,
         AbiRule::GprCallback,
         {},
         0.9,
         true,
         "znver5"},
    Gate{
        "delegate_stub_i32", TailJmp, DirectCall | Stack | Div | Idiv | Jcc, 1, AbiRule::GprCallback, {}, 0.6, false, "znver5"},
    Gate{"delegate_stub_span",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         1,
         AbiRule::GprCallback,
         {},
         0.6,
         false,
         "znver5"},
    Gate{"delegate_stub_index",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         1,
         AbiRule::GprCallback,
         {},
         0.6,
         false,
         "znver5"},
    Gate{"delegate_stub_nonprimary",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         1,
         AbiRule::GprCallback,
         {},
         0.6,
         false,
         "znver5"},
    Gate{"delegate_invoke_large_rref",
         TailJmp,
         DirectCall | Stack | Div | Idiv | Jcc,
         3,
         AbiRule::GprCallback,
         {},
         0.7,
         false,
         "znver5"},
    Gate{"delegate_checked_i32",
         Jcc | TailJmp,
         DirectCall | Stack | Div | Idiv,
         8,
         AbiRule::GprCallback,
         {},
         1.6,
         false,
         "znver5"},
    Gate{"text_eq16", 0, Jcc | Call | Stack, 8, AbiRule::Registers, {}, 1.5},
    Gate{"text_cmp16", 0, Call | Stack, 0, AbiRule::Registers, {}, 3.6},
    Gate{"hash_fnv1a64_16", 0, Call | Div | Idiv | Stack, 0, AbiRule::Registers, {}, 16.0},
    Gate{"bits_with_flags3", 0, Jcc | Movabs | Call | Stack, 3, AbiRule::Registers, {}, 1.0},
    Gate{"bits_all_of3", 0, Jcc | Movabs | Call | Stack, 4, AbiRule::Registers, {}, 1.2},
    Gate{"cold_invoke_pair", 0, Stack, 0, AbiRule::Registers, {}, 1.5},
    Gate{"register_tier", 0, 0, 0, AbiRule::Registers, {}, 0.5},
    Gate{"register_pair", 0, 0, 0, AbiRule::Registers, {}, 0.8},
    Gate{"register_small", 0, 0, 0, AbiRule::Registers, {}, 0.8},
    Gate{"register_c_abi", 0, 0, 0, AbiRule::Registers, {}, 2.8},
    Gate{"pool_index_slot", 0, 0, 0, AbiRule::Registers, {}, 0.5},
    Gate{"register_pressure_control", 0, 0, 0, AbiRule::MemoryControl},
    Gate{"memory_control_non_trivial_dtor", 0, 0, 0, AbiRule::MemoryControl},
    Gate{"memory_control_cache_aligned", 0, 0, 0, AbiRule::MemoryControl},
    Gate{"bitword_low_matches", 0, Jcc | Movabs | Call, 4, AbiRule::None, {}, 1.2},
    Gate{"bitword_high_matches", 0, Jcc | Call, 0, AbiRule::None, {}, 2.5},
    Gate{"bit_ones_sum", Tzcnt | Blsr, 0, 0, AbiRule::None, {}, 2.0},
    Gate{"flow_select_register_u64", Cmov, Jcc | Call, 0, AbiRule::None, {}, 0.8},
    Gate{"flow_select_u64", Cmov, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 0.8},
    Gate{"flow_likely"},
    Gate{"flow_unlikely"},
    Gate{"cold_sink_fn", 0, 0, 0, AbiRule::None, ".text.unlikely"},
    Gate{"cold_sink_body", 0, 0, 0, AbiRule::None, ".text.unlikely"},
    Gate{"fast_divisor_u64_div", 0, Div | Idiv | Udivti3, 0, AbiRule::None, {}, 4.0},
    Gate{"fast_divisor_u32_rem", 0, Div | Idiv | Udivti3, 0, AbiRule::Registers, {}, 4.0},
    Gate{"pool_prefetch_l1", PrefetchT0, 0, 0, AbiRule::None, {}, 0.7, true},
    Gate{"uarch_arch_prefetch_write", Prefetchw, 0, 0, AbiRule::None, {}, 0.7, true},
    Gate{"uarch_arch_scalar_gather", 0, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 4.2},
    Gate{"x86_v3_gather", Pext, 0, 0, AbiRule::None, {}, 1.0},
    Gate{"x86_v3_slow_pdep_gather", Tzcnt | Blsr, Pext | Pdep, 0, AbiRule::None, {}, 3.0},
    Gate{"x86_v3_slow_pdep_scatter", Tzcnt | Blsr, Pext | Pdep, 0, AbiRule::None, {}, 2.7},
    Gate{"uarch_arch_masked_sum", Simd | Compare, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 3.2, true},
    Gate{"uarch_arch_first_ge", Simd | Compare, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 2.4, true},
    Gate{"uarch_arch_v4_gather", Pext, 0, 0, AbiRule::None, {}, 1.0},
    Gate{"uarch_arch_v3_scatter", Pdep, 0, 0, AbiRule::None, {}, 1.0},
    Gate{"bitpatch_apply", 0, Jcc | Movabs | Call | Stack, 0, AbiRule::None, {}, 2.0},
    Gate{"bitmap64_pop_lowest", 0, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 1.8},
    Gate{"fast_divisor_u64_rem", 0, Div | Idiv | Udivti3 | Call, 0, AbiRule::None, {}, 4.0},
    Gate{"fast_divisor_u64_pow2_rem", 0, Div | Idiv | Udivti3 | Call, 0, AbiRule::None, {}, 0.7},
    Gate{"power_of_two_align", 0, Jcc | Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 1.0},
    Gate{"saturating_add_u64", 0, Jcc | Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 0.8},
    Gate{"pool_get_masked", 0, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 0.7, true},
    Gate{"uarch_arch_v4_masked_sum", Simd | Compare, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 2.4, true},
    Gate{"uarch_arch_v4_first_ge", Simd | Compare, Call | Div | Idiv | Stack, 0, AbiRule::None, {}, 2.0, true}};
}  // namespace uarch::probes
