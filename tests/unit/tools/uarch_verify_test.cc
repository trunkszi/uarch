#include <algorithm>
#include <array>
#include <cmath>
#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <uarch/core/arithmetic/fast_divisor_u64.hpp>
#include <uarch/core/mem/pool_index.hpp>
#include <uarch_verify_config.hpp>

#include "verify.hpp"
using namespace uarch::verify;
using namespace uarch::probes;
extern "C" std::uint64_t bitpatch_apply(std::uint64_t);
extern "C" std::uint64_t bitmap64_pop_lowest(std::uint64_t);
extern "C" std::uint64_t fast_divisor_u64_rem(uarch::arithmetic::FastDivisorU64, std::uint64_t);
extern "C" std::uint64_t fast_divisor_u64_pow2_rem(uarch::arithmetic::FastDivisorU64, std::uint64_t);
extern "C" std::uint64_t power_of_two_align(std::uint64_t);
extern "C" std::uint64_t saturating_add_u64(std::uint64_t, std::uint64_t);
extern "C" std::uint64_t pool_get_masked(std::uint64_t*, uarch::mem::PoolIndex<std::uint64_t>);
#if defined(__x86_64__)
extern "C" std::int64_t uarch_arch_v4_masked_sum(const std::int64_t*, const std::int64_t*, std::int64_t);
extern "C" std::uint32_t uarch_arch_v4_first_ge(const std::int64_t*, std::int64_t);
#endif
namespace {
bool expanded_probe_controls() {
  if (bitpatch_apply(0) != 0x905 || bitpatch_apply(UINT64_MAX) != 0xfffffffffffff9f5ULL) return false;
  if (bitmap64_pop_lowest(0) != 64 || bitmap64_pop_lowest(0x18) != 19 || bitmap64_pop_lowest(1ULL << 63) != 63) return false;
  if (fast_divisor_u64_rem(uarch::arithmetic::divisor_u64<7>(), UINT64_MAX) != 1 ||
      fast_divisor_u64_pow2_rem(uarch::arithmetic::divisor_u64<64>(), 129) != 1)
    return false;
  if (power_of_two_align(65) != 127 || saturating_add_u64(UINT64_MAX, 1) != UINT64_MAX || saturating_add_u64(3, 4) != 7)
    return false;
  std::array<std::uint64_t, 256> pool{};
  pool[1] = 17;
  if (pool_get_masked(pool.data(), *uarch::mem::PoolIndex<std::uint64_t>::try_new(257)) != 17) return false;
#if defined(__x86_64__)
  if (__builtin_cpu_supports("x86-64-v4")) {
    const std::array<std::int64_t, 4> keys{4, -1, 3, 2}, values{10, 20, 30, 40};
    if (uarch_arch_v4_masked_sum(keys.data(), values.data(), 3) != 60 || uarch_arch_v4_first_ge(keys.data(), 4) != 0 ||
        uarch_arch_v4_first_ge(keys.data(), 5) != UINT32_MAX)
      return false;
  }
#endif
  return true;
}
bool assembly_controls() {
  auto function = [](std::string_view body) {
    auto parsed = parse_assembly("Disassembly of section .text.subject:\n0000 <subject>:\n" + std::string(body));
    return parsed.at("subject");
  };
  const auto select = function(" 0: testl %edi, %edi\n 2: cmovneq %rsi, %rax\n 6: retq\n");
  if (!check_function(Gate{"subject", Cmov, Jcc | Call}, select).empty()) return false;
  for (const auto& [code, deny] :
       {std::pair{" 0: divq %rdi\n 3: retq\n", std::uint32_t{Div}}, std::pair{" 0: je 0x10\n 2: retq\n", std::uint32_t{Jcc}},
        std::pair{" 0: pextq %rsi, %rdi, %rax\n 4: retq\n", std::uint32_t{Pext}}}) {
    if (check_function(Gate{"subject", 0, deny}, function(code)).empty()) return false;
  }
  const auto avx512 = function(" 0: vpcmpgtq %ymm1, %ymm0, %k1\n 6: kmovw %k1, %eax\n 9: retq\n");
  if (!check_function(Gate{"subject", Simd | Compare, Call | Stack}, avx512).empty()) return false;
  if (check_function(Gate{"subject", Compare}, function(" 0: kmovw %k1, %eax\n 4: retq\n")).empty()) return false;
  const auto bare = function(" 0: retq\n");
  if (check_function(Gate{"subject", Cmov}, bare).empty() || check_function(Gate{"subject", Prefetchw}, bare).empty())
    return false;
  if (!check_function(Gate{"subject", Prefetchw}, function(" 0: prefetchw (%rdi)\n 3: retq\n")).empty()) return false;
  if (check_function(Gate{"subject", PrefetchT0}, bare).empty()) return false;
  if (!check_function(Gate{"subject", PrefetchT0}, function(" 0: prefetcht0 (%rdi)\n 3: retq\n")).empty()) return false;
  if (check_function(Gate{"subject", 0, 0, 1}, select).empty()) return false;
  if (check_function(Gate{"subject", 0, 0, 0, AbiRule::None, ".text.unlikely"}, select).empty()) return false;
  if (!stack_parameter(function(" 0: movq 0x8(%rsp), %rax\n 4: retq\n"))) return false;
  if (stack_parameter(function(" 0: movq %rax, 0x8(%rsp)\n 4: retq\n"))) return false;
  if (stack_parameter(function(" 0: movq -0x8(%rsp), %rax\n 4: retq\n"))) return false;
  if (stack_parameter(function(" 0: leaq 0x8(%rsp), %rax\n 4: retq\n"))) return false;
  if (!stack_parameter(function(" 0: movq 0x10(%rbp), %rax\n 4: retq\n"))) return false;
  const auto relocation = function(" 0: callq 0x5\n 1: R_X86_64_PLT32 __udivti3-0x4\n 5: retq\n");
  if (check_function(Gate{"subject", 0, Udivti3}, relocation).empty()) return false;
  return true;
}
bool ir_controls() {
  const std::string good = "define i64 @subject(i64 %ptr) {\n ret i64 %ptr\n}\n";
  if (!check_signature(ir_signature(good, "subject"), AbiRule::Registers)) return false;
  for (auto bad :
       {"define i64 @subject(ptr %x) {", "define void @subject(ptr sret(%S) %x) {", "define i64 @subject(ptr byval(%S) %x) {",
        "define x86_fp80 @subject(x86_fp80 %x) {", "define i64 @subject(i64 dead_on_unwind %x) {"})
    if (check_signature(ir_signature(bad, "subject"), AbiRule::Registers)) return false;
  if (check_signature(ir_signature(good, "subject"), AbiRule::MemoryControl)) return false;
  if (!check_signature(ir_signature("define i64 @subject(ptr byval({i64,i64}) %x) {", "subject"), AbiRule::MemoryControl))
    return false;
  return !check_signature(ir_signature(good, "missing"), AbiRule::Registers);
}
bool lint_controls() {
  for (auto bad : {"#define BAD 1\n", "auto x = result.value();", "std::function<void()> callback;", "#include <iostream>\n",
                   "throw error;", "try {} catch(...) {}", "auto x=detail::TokenFactory::make<Scalar>();",
                   "auto x=reinterpret_cast<uarch::arch::X86V3*>(bytes);"})
    if (lint_text("include/negative.hpp", bad).empty()) return false;
  if (!lint_text("include/positive.hpp",
                 "// throw try catch TokenFactory\nconst char* s=R\"raw(std::function<int()> #define x 1)raw\";")
           .empty())
    return false;
  if (lint_text("src/long.cc", std::string(801, '\n')).empty()) return false;
  if (lint_text("tests/nonascii.cc", std::string(1, static_cast<char>(0xff))).empty()) return false;
  if (!lint_text("include/uarch/arch/issuer.hpp", "struct TokenFactory;").empty()) return false;
  if (!lint_text("src/detect/issuer.cc", "TokenFactory::make<Scalar>();").empty()) return false;
  for (auto prefix : {"L", "u", "U", "u8"}) {
    const auto text = "auto q = " + std::string(prefix) + R"code('"'; return o.value(); const char* s = "x";)code";
    if (lint_text("include/negative.hpp", text).empty()) return false;
    const auto literal = "auto q = " + std::string(prefix) + R"code('"'; const char* s = "o.value()";)code";
    if (!lint_text("include/positive.hpp", literal).empty()) return false;
  }
  if (lint_text("include/negative.hpp", "0xFF'FF; return o.value();").empty()) return false;
  if (!lint_text("tests/reflection.cc", "std::meta::members_of(^^X, std::meta::access_context::current());").empty())
    return false;
  for (auto path :
       {"include/uarch/core/layout/example.hpp", "include/uarch/core/bits/example.hpp", "tests/unit/core/reflect_test.cc"}) {
    if (!lint_text(path, "uarch::reflect::unchecked_context();").empty()) return false;
    if (lint_text(path, "std::meta::access_context::unchecked();").empty()) return false;
    if (lint_text(path, "using A = std::meta::access_context; auto c = A::unchecked();").empty()) return false;
  }
  return std::ranges::all_of(std::array{"include/uarch/arch/example.hpp", "src/detect/example.cc",
                                        "include/uarch/tiers/example.hpp", "tests/unit/arch/example.cc"},
                             [](auto path) { return !lint_text(path, "uarch::reflect::unchecked_context();").empty(); });
}
}  // namespace
bool remediation_controls() {
  auto function = [](std::string_view body) {
    return parse_assembly("Disassembly of section .text.subject:\n0000 <subject>:\n" + std::string(body)).at("subject");
  };
  for (auto body : {" 0: jmp 4b <helper>\n", " 0: jmpq *%rax\n", " 0: b 0x40 <helper>\n", " 0: br x16\n"}) {
    if (check_function(Gate{"subject", 0, Call}, function(body)).empty()) return false;
  }
  for (auto body : {" 0: udiv x0, x1, x2\n", " 0: sdiv x0, x1, x2\n"}) {
    if (check_function(Gate{"subject", 0, Div}, function(body)).empty()) return false;
  }
  return std::ranges::all_of(
      std::array{"auto n=0xFF'FF; auto v=result.value(); auto c='c';", "auto n=0xAB'CD; TokenFactory::make(); auto c='c';",
                 "auto n=1'000'000; std::meta::access_context::unchecked();", "auto n=0xFF'FF; unchecked_context();",
                 "using uarch::reflect::unchecked_context;", "#\\\ndefine BAD 1\n"},
      [](auto text) { return !lint_text("include/negative.hpp", text).empty(); });
}
bool extent_and_abi_controls() {
  const auto parsed =
      parse_assembly("Disassembly of section .text.subject:\n0000 <subject>:\n 0: retq\n 1: pextq %rsi, %rdi, %rax\n",
                     SymbolExtents{{"subject", {0, 1}}});
  const auto& bounded = parsed.at("subject");
  if (bounded.instructions.size() != 1 || check_function(Gate{"subject", Pext}, bounded).empty()) return false;
  auto value = [](std::string_view body) {
    return parse_assembly("Disassembly of section .text.value:\n0000 <value>:\n" + std::string(body)).at("value");
  };
  if (!indirect_parameter(value(" 0: movl (%rdi), %eax\n 2: retq\n"))) return false;
  if (!indirect_parameter(value(" 0: ldr x0, [x1]\n 4: ret\n"))) return false;
  if (indirect_parameter(value(" 0: leaq 8(%rdi), %rax\n 4: retq\n"))) return false;
  for (auto body : {" 0: jmp 4b <value+0x10>\n", " 0: b .Llocal\n"})
    if (!check_function(Gate{"value", 0, Call}, value(body)).empty()) return false;
  for (auto text : {"Name: .eh_frame\n", "Name: __cxa_atexit", "Name: __cxa_guard_acquire", "Name: __cxa_guard_release"})
    if (exception_artifacts(text)) return false;
  for (auto text :
       {"Name: .gcc_except_table.foo", "Name: __cxa_throw", "Name: __cxa_rethrow", "Name: __cxa_begin_catch",
        "Name: __cxa_end_catch", "Name: __cxa_allocate_exception", "Name: __cxa_free_exception", "Name: __gxx_personality_v0"})
    if (!exception_artifacts(text)) return false;
  if (lint_text("include/negative.hpp", "#\\\r\ndefine BAD 1\n").empty()) return false;
  if (!lint_text("include/uarch/core/reflect.hpp", "std::meta::access_context::unchecked();").empty()) return false;
  return true;
}
bool object_controls(const Context& context) {
#if defined(__x86_64__)
  std::string temporary = "/tmp/uarch-verify-test-XXXXXX";
  if (!::mkdtemp(temporary.data())) return false;
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{temporary};
  const auto object = (cleanup.path / "negative.o").string();
  const std::string source = R"(
.section .text.bounded,"ax",@progbits
.globl bounded
.type bounded,@function
bounded:
retq
.size bounded,.-bounded
pextq %rsi,%rdi,%rax
.section .text.tail,"ax",@progbits
.globl tail
.type tail,@function
tail:
jmp external_helper
.size tail,.-tail
.section .text.indirect,"ax",@progbits
.globl indirect
.type indirect,@function
indirect:
movl (%rdi),%eax
retq
.size indirect,.-indirect
.section .gcc_except_table,"a",@progbits
.byte 1
)";
  auto compiled = command({(context.tools / "clang++").string(), "-x", "assembler", "-c", "-", "-o", object}, source);
  if (!compiled.ok) return false;
  auto inspected = inspect_object(context, object);
  if (!inspected.ok) return false;
  if (inspected.functions.at("bounded").instructions.size() != 1 ||
      check_function(Gate{"bounded", Pext}, inspected.functions.at("bounded")).empty())
    return false;
  if (check_function(Gate{"tail", 0, Call}, inspected.functions.at("tail")).empty()) return false;
  if (!indirect_parameter(inspected.functions.at("indirect"))) return false;
  auto mutated = context;
  mutated.objects.push_back(object);
  const auto size = size_gate(mutated);
  if (size.status != Status::Fail || size.detail.find("exception artifact") == std::string::npos) return false;
  auto layout = read(context.root / "src/probes/layout_probes.cc");
  const std::string signature = "register_small(RegisterSmall value)";
  auto at = layout.find(signature);
  if (at == std::string::npos) return false;
  layout.replace(at, signature.size(), "register_small(const RegisterSmall* value)");
  at = layout.find("value.value", at);
  if (at == std::string::npos) return false;
  layout.replace(at, std::string("value.value").size(), "value->value");
  const std::filesystem::path gcc = "/root/.local/toolchain/gcc-glibc/bin/x86_64-unknown-linux-gnu-g++";
  const bool has_gcc = std::filesystem::is_regular_file(gcc);
  std::vector<std::string> args{has_gcc ? gcc.string() : (context.tools / "clang++").string(), "-std=c++26",
                                has_gcc ? "-freflection" : "-freflection-latest"};
  if (!has_gcc) args.emplace_back("-stdlib=libc++");
  const auto bad_object = (cleanup.path / "layout-pointer.o").string();
  for (const auto& arg :
       std::vector<std::string>{"-fno-exceptions", "-fno-rtti", "-O3", "-fno-lto", "-I" + (context.root / "include").string(),
                                "-x", "c++", "-", "-c", "-o", bad_object})
    args.emplace_back(arg);
  if (!command(args, layout).ok) return false;
  auto bad_context = Context(context);
  bool replaced = false;
  for (auto& path : bad_context.objects) {
    auto existing = inspect_object(context, path);
    if (!existing.ok) return false;
    if (existing.functions.contains("register_small")) {
      path = bad_object;
      replaced = true;
      break;
    }
  }
  if (!replaced) return false;
  const auto abi = abi_gate(bad_context);
  if (abi.status != Status::Fail ||
      abi.detail.find("native stack/indirect value parameter register_small") == std::string::npos)
    return false;

#endif
  return true;
}
bool checksum_controls() {
  std::uint64_t at_five = 0, at_six = 0;
  for (std::uint64_t i = 0; i < 200000; ++i) {
    at_five = combine_probe_checksum(at_five, i, (i & 15) == 5);
    at_six = combine_probe_checksum(at_six, i, (i & 15) == 6);
  }
  const auto forward = combine_probe_checksum(combine_probe_checksum(0, 0, 7), 1, 11);
  const auto swapped = combine_probe_checksum(combine_probe_checksum(0, 0, 11), 1, 7);
  return at_five != at_six && forward != swapped;
}
bool tma_controls() {
  PmuCounters raw;
  raw.cycles = 100;
  raw.instructions = 400;
  raw.branches = 40;
  raw.branch_misses = 1;
  raw.slot_width = 8;
  raw.has_raw_slots = true;
  raw.retired_ops = 200;
  raw.dispatched_ops = 300;
  raw.frontend_no_ops_slots = 200;
  raw.backend_stall_slots = 300;
  auto metrics = compute_tma_metrics(raw, 100);
  if (metrics.retiring_pct != 25 || metrics.bad_speculation_pct != 12.5 || metrics.frontend_bound_pct != 25 ||
      metrics.backend_bound_pct != 37.5 || metrics.dominant_bound != "Backend Bound" || metrics.ipc != 4 ||
      metrics.cycles_per_op != 1)
    return false;
  raw.retired_ops = 600;
  raw.dispatched_ops = 700;
  raw.frontend_no_ops_slots = 50;
  raw.backend_stall_slots = 50;
  if (compute_tma_metrics(raw, 100).dominant_bound != "Retiring") return false;
  raw.retired_ops = 100;
  raw.dispatched_ops = 600;
  if (compute_tma_metrics(raw, 100).dominant_bound != "Bad Speculation") return false;
  raw.retired_ops = 300;
  raw.dispatched_ops = 400;
  raw.frontend_no_ops_slots = 100;
  raw.backend_stall_slots = 0;
  metrics = compute_tma_metrics(raw, 100);
  if (metrics.retiring_pct != 60 || metrics.bad_speculation_pct != 20 || metrics.frontend_bound_pct != 20 ||
      metrics.backend_bound_pct != 0)
    return false;
  raw.frontend_no_ops_slots = 0;
  raw.backend_stall_slots = 0;
  raw.dispatched_ops = raw.retired_ops;
  if (compute_tma_metrics(raw, 100).retiring_pct != 100) return false;
  if (!zen_raw_slots_supported(0x1a, 0) || !zen_raw_slots_supported(0x19, 0x10) || !zen_raw_slots_supported(0x19, 0x1f) ||
      !zen_raw_slots_supported(0x19, 0x60) || !zen_raw_slots_supported(0x19, 0x7f) || zen_raw_slots_supported(0x19, 0x0f) ||
      zen_raw_slots_supported(0x19, 0x20) || zen_raw_slots_supported(0x19, 0x5f) || zen_raw_slots_supported(0x19, 0x80) ||
      zen_raw_slots_supported(0x17, 0x10))
    return false;
  PmuCounters generic;
  generic.cycles = 100;
  generic.stalled_frontend = 80;
  generic.stalled_backend = 20;
  metrics = compute_tma_metrics(generic, 100);
  if (metrics.frontend_bound_pct != 80 || metrics.backend_bound_pct != 20 || metrics.dominant_bound != "Frontend Bound")
    return false;
  generic.stalled_frontend = 10;
  generic.stalled_backend = 90;
  if (compute_tma_metrics(generic, 100).dominant_bound != "Backend Bound") return false;
  generic.instructions = 200;
  generic.branch_misses = 2;
  metrics = compute_tma_metrics(generic, 100);
  if (std::abs(metrics.retiring_pct + metrics.bad_speculation_pct + metrics.frontend_bound_pct + metrics.backend_bound_pct -
               100) > 1e-9 ||
      metrics.instructions_per_op != 2 || metrics.branch_misses_per_op != 0.02)
    return false;
  if (compute_tma_metrics({}, 0).cycles_per_op != 0) return false;
  const std::string csv =
      "1000;;cycles:u;100;100.00;\n2000;;instructions:u;100;100.00;\n100;;branches:u;100;100.00;\n2;;branch-misses:u;100;100."
      "00;\n";
  const auto parsed = parse_perf_stat_csv(csv);
  if (!parsed || parsed->cycles != 1000 || parsed->instructions != 2000 || parsed->branch_misses != 2 ||
      parsed->min_active_ratio != 1)
    return false;
  for (auto bad : {"<not supported>", "<not counted>", "-1", "nan", "inf"}) {
    const auto text = std::string(bad) + ";;instructions:u;100;100;\n0;;branch-misses:u;100;100;\n";
    if (parse_perf_stat_csv(text)) return false;
  }
  if (parse_perf_stat_csv("1;;instructions:u;100;100;\n")) return false;
  if (parse_perf_stat_csv(csv + "1;;instructions:u;100;100;\n")) return false;
  const auto raw_csv =
      parse_perf_stat_csv(csv + "300;;r00c1:u;100;100;\n400;;r07aa:u;100;100;\n100;;r01a0:u;100;100;\n0;;r1ea0:u;100;100;\n");
  if (!raw_csv || !raw_csv->has_raw_slots || compute_tma_metrics(*raw_csv, 100).backend_bound_pct != 0) return false;
  const auto partial_raw = parse_perf_stat_csv(
      csv + "300;;r00c1:u;100;100;\n400;;r07aa:u;100;100;\n100;;r01a0:u;100;100;\n<not supported>;;r1ea0:u;100;100;\n");
  if (!partial_raw || partial_raw->has_raw_slots) return false;
  const auto multiplexed = parse_perf_stat_csv("1;;instructions:u;100;98.5;\n0;;branch-misses:u;100;100;\n");
  return multiplexed && multiplexed->min_active_ratio < 0.99;
}
bool mca_controls(const Context& context) {
  for (auto required : {"register_tier",
                        "register_pair",
                        "register_small",
                        "register_c_abi",
                        "pool_index_slot",
                        "pool_prefetch_l1",
                        "uarch_arch_prefetch_write",
                        "uarch_arch_scalar_gather",
                        "x86_v3_slow_pdep_scatter",
                        "uarch_arch_v4_gather",
                        "uarch_arch_v3_scatter",
                        "bitpatch_apply",
                        "bitmap64_pop_lowest",
                        "fast_divisor_u64_rem",
                        "fast_divisor_u64_pow2_rem",
                        "power_of_two_align",
                        "saturating_add_u64",
                        "pool_get_masked",
                        "uarch_arch_v4_masked_sum",
                        "uarch_arch_v4_first_ge"}) {
    if (std::ranges::none_of(gates, [&](const auto& gate) { return gate.symbol == required && gate.max_throughput > 0; }))
      return false;
  }
  const auto* old = std::getenv("UARCH_MCA_CPU");
  const std::optional<std::string> saved = old ? std::optional<std::string>(old) : std::nullopt;
  ::setenv("UARCH_MCA_CPU", "znver5,sapphirerapids", 1);
  auto configured = mca_cpu_models(context);
  const auto result = mca_gate(context);
  if (saved)
    ::setenv("UARCH_MCA_CPU", saved->c_str(), 1);
  else
    ::unsetenv("UARCH_MCA_CPU");
  if (configured != std::vector<std::string>{"znver5", "sapphirerapids"}) return false;
  auto overridden = context;
  overridden.mca_cpus = {"skylake"};
  if (mca_cpu_models(overridden) != std::vector<std::string>{"skylake"}) return false;
  if (result.status != Status::Pass) {
    std::fputs(result.detail.c_str(), stderr);
    return false;
  }
  for (const auto& gate : uarch::probes::gates)
    if (gate.max_throughput > 0)
      for (const auto& cpu : configured) {
        const auto begin = result.detail.find(std::string(gate.symbol) + " cpu=" + cpu);
        if (begin == std::string::npos) return false;
        const auto row = result.detail.substr(begin, result.detail.find('\n', begin) - begin);
        const auto limit = gate.max_throughput * (cpu == gate.mca_cpu ? 1.0 : 1.25);
        if (row.find("budget=" + std::to_string(limit) + " ") == std::string::npos) return false;
      }
  auto reference = context;
  reference.mca_cpus = {"znver4"};
  const auto reference_result = mca_gate(reference);
  if (reference_result.status != Status::Pass) return false;
  for (const auto& gate : gates) {
    if (gate.max_throughput <= 0) continue;
    const auto begin = reference_result.detail.find(std::string(gate.symbol) + " cpu=" + gate.mca_cpu);
    if (begin == std::string::npos) return false;
    const auto row = reference_result.detail.substr(begin, reference_result.detail.find('\n', begin) - begin);
    if (row.find("budget=" + std::to_string(gate.max_throughput) + " ") == std::string::npos) return false;
  }
  for (auto bad : {"", ",znver4", "znver4,", "znver4, ", "znver4,,znver5"}) {
    overridden.mca_cpus = {bad};
    if (!mca_cpu_models(overridden).empty()) return false;
  }
  std::string scratch = "/tmp/uarch-mca-unbudgeted-XXXXXX";
  const int descriptor = ::mkstemp(scratch.data());
  if (descriptor < 0) return false;
  ::close(descriptor);
  const auto compiled = command(
      {(context.tools / "clang++").string(), "-x", "assembler", "-c", "-", "-o", scratch},
      ".text\n.globl cold_sink_fn\n.type cold_sink_fn,@function\ncold_sink_fn:\nret\n.size cold_sink_fn,.-cold_sink_fn\n");
  auto extra = context;
  extra.objects.push_back(scratch);
  extra.mca_cpus = configured;
  const auto ignored = compiled.ok ? mca_gate(extra) : Result{"mca-gate", Status::Fail, compiled.output};
  std::filesystem::remove(scratch);
  if (ignored.status != Status::Pass) return false;
  auto missing = context;
  missing.objects.clear();
  if (mca_gate(missing).status != Status::Fail) return false;
  return result.detail.find("Total uOps=") != std::string::npos && result.detail.find("Dispatch Width=") != std::string::npos;
}
namespace {
struct Environment {
  std::string name{};
  std::optional<std::string> old{};
  explicit Environment(const char* key) : name(key) {
    if (const auto* p = std::getenv(key)) old = p;
  }
  ~Environment() {
    if (old)
      ::setenv(name.c_str(), old->c_str(), 1);
    else
      ::unsetenv(name.c_str());
  }
};
std::string stats_fixture(double n, bool modern = false) {
  const std::string p = modern ? "board.processor.cores0.core." : "system.cpu.";
  std::ostringstream out;
  out << p << "numCycles " << 100000 + 10 * n << '\n'
      << p << (modern ? "commitStats0.numInsts " : "commit.instsCommitted::total ") << 1000 + 2 * n << '\n'
      << p << (modern ? "commitStats0.numOps " : "commit.opsCommitted::total ") << 1000 + 2 * n << '\n'
      << p << "rename.renamedInsts " << 1000 + 2 * n << '\n'
      << p << "rename.idleCycles 0\n"
      << p << "rename.squashCycles 0\n"
      << p << "commit.branchMispredicts 0\n"
      << p << "iew.memOrderViolationEvents 0\n";
  return out.str();
}
bool gem5_parser_controls() {
  const auto stages =
      stats_fixture(512) +
      "system.cpu.idleCycles 7\nsystem.cpu.decode.idleCycles 11\nsystem.cpu.fetch.idleCycles 12\nsystem.cpu.iew.idleCycles "
      "13\nsystem.cpu.decode.squashCycles 14\nsystem.cpu.dcache.overallMisses::total 3\n";
  auto stage_stats = parse_gem5_stats(stages);
  if (!stage_stats || stage_stats->idle_cycles != 7 || stage_stats->dcache_misses != 3) return false;
  auto a = parse_gem5_stats(stats_fixture(512)), b = parse_gem5_stats(stats_fixture(2560, true)),
       c = parse_gem5_stats(stats_fixture(4608));
  if (!a || !b || !c || !a->has_rename_slots || !a->has_structure) return false;
  auto absent = *b;
  absent.has_structure = false;
  auto partial = diff_gem5_counters(absent, *a);
  auto partial_window = check_gem5_linearity(*a, 512, absent, 2560, *c, 4608);
  if (!partial || partial->has_structure || !partial_window || partial_window->has_structure) return false;
  auto two = diff_gem5_counters(*b, *a);
  auto three = check_gem5_linearity(*a, 512, *b, 2560, *c, 4608);
  if (!two || !three || two->num_cycles != 20480 || three->committed_ops != 4096) return false;
  auto noisy_low = *a, noisy_high = *b;
  noisy_low.commit_squashed_insts = 5023;
  noisy_high.commit_squashed_insts = 5018;
  const auto noise_delta = diff_gem5_counters(noisy_high, noisy_low);
  if (!noise_delta || noise_delta->commit_squashed_insts != 0) return false;
  for (auto member : {&Gem5Counters::idle_cycles, &Gem5Counters::rename_squash_cycles, &Gem5Counters::branch_mispredicts,
                      &Gem5Counters::mem_order_violations}) {
    auto lower = *a, higher = *b;
    lower.*member = 1;
    if (diff_gem5_counters(higher, lower)) return false;
  }
  auto drift = *c;
  drift.num_cycles += 4096;
  if (check_gem5_linearity(*a, 512, *b, 2560, drift, 4608) || diff_gem5_counters(*a, *b)) return false;
  if (check_gem5_linearity(*a, 512, *b, 512, *c, 4608)) return false;
  const auto m = compute_gem5_metrics(*three, 2048, 6);
  if (!m || m->cycles_per_op != 10 || m->uops_per_op != 2 || m->attribution_mode != "o3-rename-slot") return false;
  auto pressure = *three;
  pressure.rob_full_events = 1000000;
  pressure.iq_full_events = 1000000;
  auto pm = compute_gem5_metrics(pressure, 2048, 6);
  if (!pm || pm->backend_bound_pct != m->backend_bound_pct || pm->rob_full_per_op == 0) return false;
  auto inconsistent = *three;
  inconsistent.renamed_insts = 100000000;
  if (compute_gem5_metrics(inconsistent, 2048, 6) || compute_gem5_metrics(*three, 0, 6)) return false;
  for (const auto* bad : {"nan", "inf", "-1"})
    if (parse_gem5_stats("system.cpu.numCycles " + std::string(bad) +
                         "\nsystem.cpu.committedInsts 1\nsystem.cpu.committedOps 1\n"))
      return false;
  if (parse_gem5_stats("system.cpu.numCycles 5\n") ||
      parse_gem5_stats(stats_fixture(512) + "board.processor.cores.core.numCycles 10\n") ||
      parse_gem5_stats(stats_fixture(512) + "system.cpu.numCycles 99\n"))
    return false;
  for (auto extra :
       {"system.cpu1.numCycles 10\n", "system.switch_cpus.numCycles 10\n", "system.cpu.numCycles 105120\n",
        "Begin Simulation Statistics\n",
        "Begin Simulation Statistics\nEnd Simulation Statistics\nBegin Simulation Statistics\nEnd Simulation Statistics\n"})
    if (parse_gem5_stats(stats_fixture(512) + extra)) return false;
  const auto v26 = parse_gem5_stats(
      "Begin Simulation Statistics\nsystem.cpu.numCycles 100\nsystem.cpu.commitStats0.numInsts 20\n"
      "system.cpu.commitStats0.numOps 30\nsystem.cpu.rename.renamedInsts 31\n"
      "system.cpu.rename.status::Idle 2\nsystem.cpu.rename.status::Squashing 3\n"
      "system.cpu.commit.branchMispredicts 4\nsystem.cpu.branchPred.condIncorrect 9\nEnd Simulation Statistics\n");
  if (!v26 || !v26->has_rename_slots || v26->branch_mispredicts != 4 || v26->rename_idle_cycles != 2 ||
      v26->rename_squash_cycles != 3)
    return false;
  auto alias = stats_fixture(512, true);
  const auto pos = alias.find("cores0.core");
  while (alias.find("cores0.core") != std::string::npos) alias.replace(alias.find("cores0.core"), 11, "cores.core");
  if (pos == std::string::npos || !parse_gem5_stats(alias)) return false;
  return !reference_probe_checksum("unknown", 64) && reference_probe_checksum("bitword_low_matches", 0) == 0;
}
bool gem5_execution_controls(const Context& context) {
  Environment binary("UARCH_GEM5_BIN"), models("UARCH_GEM5_CPU"), mode("UARCH_GEM5_TEST_MODE"),
      budget("UARCH_GEM5_MAX_CYCLES_PER_OP");
  ::unsetenv("UARCH_GEM5_MAX_CYCLES_PER_OP");
  ::unsetenv("UARCH_GEM5_CPU");
  Environment expected_checksum("UARCH_GEM5_EXPECTED_CHECKSUM");
  const auto checksum = reference_probe_checksum("bit_ones_sum", 512);
  if (!checksum) return false;
  ::setenv("UARCH_GEM5_EXPECTED_CHECKSUM", std::to_string(*checksum).c_str(), 1);
  auto configured = context;
  configured.gem5_cpus = {"zen4-o3,znver5,sapphirerapids,neoverse-v2-o3"};
  if (gem5_cpu_models(configured) !=
      std::vector<std::string>{"o3-w6-rob320", "o3-w8-rob448", "o3-w6-rob512", "o3-a64-w6-rob320"})
    return false;
  for (auto bad : {"", ",zen4-o3", "zen4-o3,", "bogus", "znver4,,znver5"}) {
    configured.gem5_cpus = {bad};
    if (!gem5_cpu_models(configured).empty()) return false;
  }
  for (auto probe : {"bitword_low_matches", "bitword_high_matches", "flow_select_register_u64", "flow_select_u64",
                     "fast_divisor_u64_div", "fast_divisor_u32_rem", "uarch_arch_scalar_gather"})
    for (auto n : {512ULL, 2560ULL, 4608ULL}) {
      const auto result = command({std::string(config::gem5_probe), "--probe", probe, "--iterations", std::to_string(n)});
      const auto expected = reference_probe_checksum(probe, n);
      auto padded = [](std::uint64_t value) {
        auto digits = std::to_string(value);
        return std::string(20 - digits.size(), '0') + digits;
      };
      if (!result.ok || !expected ||
          result.output !=
              "UARCH_GEM5 probe=" + std::string(probe) + " n=" + padded(n) + " checksum=" + padded(*expected) + "\n")
        return false;
    }
  if (command({std::string(config::gem5_probe), "--probe", "unknown", "--iterations", "1"}).ok) return false;
  if (!command({std::string(config::gem5_probe), "--probe", "bitword_low_matches", "--iterations", "0x00000200"}).ok)
    return false;
  std::string pattern = "/tmp/uarch-gem5-mock-XXXXXX";
  const auto* path = ::mkdtemp(pattern.data());
  if (!path) return false;
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::filesystem::remove_all(path); }
  } cleanup{path};
  const auto script = cleanup.path / "gem5.opt";
  {
    std::ofstream out(script);
    out << R"py(#!/usr/bin/env python3
import os, pathlib, subprocess, sys
args=sys.argv[1:]
def value(flag): return args[args.index(flag)+1]
out=next(x.split('=',1)[1] for x in args if x.startswith('--outdir='))
probe=value('--probe'); n=int(value('--iterations'),10); mode=os.getenv('UARCH_GEM5_TEST_MODE','pass')
if not (len(value('--iterations'))==10 and os.getenv('GLIBC_TUNABLES')=='glibc.pthread.rseq=0'): sys.exit(2)
if probe in ('bit_ones_sum','x86_v3_gather','x86_v3_slow_pdep_gather','uarch_arch_masked_sum','uarch_arch_first_ge'):
    if mode=='extended-checksum':
        print(f'UARCH_GEM5 probe={probe} n={n} checksum=0');sys.exit(0)
    if mode=='extended-stats':
        print(f'UARCH_GEM5 probe={probe} n={n} checksum='+os.environ['UARCH_GEM5_EXPECTED_CHECKSUM'])
        pathlib.Path(out,'stats.txt').write_text('invalid stats\n');sys.exit(0)
    print('generic simulator trap');sys.exit(1)
if mode=='mandatory': print('generic simulator trap');sys.exit(1)
if mode=='scalar' and probe=='uarch_arch_scalar_gather': print('illegal instruction');sys.exit(1)
actual=subprocess.run([value('--binary'),'--probe',probe,'--iterations',str(n)],capture_output=True,text=True,check=True)
print(actual.stdout.strip() if mode!='checksum' else f'UARCH_GEM5 probe={probe} n={n} checksum=0')
cycles=100000+10*n+(4096 if mode=='drift' and n==4608 else 0)
text=f'system.cpu.numCycles {cycles}\nsystem.cpu.commitStats0.numInsts {1000+2*n}\nsystem.cpu.commitStats0.numOps {1000+2*n}\nsystem.cpu.rename.renamedInsts {1000+2*n}\nsystem.cpu.rename.idleCycles 0\nsystem.cpu.rename.squashCycles 0\n'
if mode=='schema': text='system.cpu.numCycles 1\n'
if mode!='missing-branch': text+=f'system.cpu.commit.branchMispredicts {n if mode=="branch" else 0}\n'
if mode!='missing-memory': text+=f'system.cpu.iew.memOrderViolationEvents {n if mode=="memory" else 0}\n'
pathlib.Path(out,'stats.txt').write_text(text)
)py";
  }
  std::filesystem::permissions(script, std::filesystem::perms::owner_all);
  ::setenv("UARCH_GEM5_BIN", script.c_str(), 1);
  ::setenv("UARCH_GEM5_TEST_MODE", "pass", 1);
  configured.gem5_cpus = {"o3-w6-rob320"};
  auto success = gem5_gate(configured);
  if (success.status != Status::Pass || success.detail.find("Unsupported") == std::string::npos) {
    std::fputs(success.detail.c_str(), stderr);
    return false;
  }
  for (auto failure :
       {"checksum", "drift", "schema", "branch", "memory", "mandatory", "scalar", "missing-branch", "missing-memory"}) {
    ::setenv("UARCH_GEM5_TEST_MODE", failure, 1);
    if (gem5_gate(configured).status != Status::Fail) return false;
  }
  for (auto failure : {"extended-checksum", "extended-stats"}) {
    ::setenv("UARCH_GEM5_TEST_MODE", failure, 1);
    const auto unsupported = gem5_gate(configured);
    const auto expected_reason = std::string_view(failure) == "extended-stats"
                                     ? "bit_ones_sum Unsupported: invalid O3 statistics"
                                     : "bit_ones_sum Unsupported: guest checksum";
    if (unsupported.status != Status::Pass || unsupported.detail.find(expected_reason) == std::string::npos) return false;
  }
  ::setenv("UARCH_GEM5_TEST_MODE", "pass", 1);
  ::setenv("UARCH_GEM5_MAX_CYCLES_PER_OP", "9", 1);
  if (gem5_gate(configured).status != Status::Fail) return false;
  ::unsetenv("UARCH_GEM5_MAX_CYCLES_PER_OP");
  ::setenv("UARCH_GEM5_BIN", "/nonexistent/gem5.opt", 1);
  if (gem5_gate(configured).status != Status::Fail) return false;
  ::unsetenv("UARCH_GEM5_BIN");
  {
    Environment search("PATH");
    ::setenv("PATH", "/nonexistent", 1);
    if (gem5_gate(configured).status != Status::NotRun) return false;
  }
  return true;
}
}  // namespace
int main() {
  if (!expanded_probe_controls()) return 2;
  if (!remediation_controls()) {
    std::fputs("remediation negative control failed\n", stderr);
    return 1;
  }
  if (!assembly_controls()) {
    std::fputs("assembly negative control failed\n", stderr);
    return 1;
  }
  if (!ir_controls()) {
    std::fputs("IR negative control failed\n", stderr);
    return 1;
  }
  if (!lint_controls()) {
    std::fputs("lint negative control failed\n", stderr);
    return 1;
  }
  const auto context = default_context();
  if (!gem5_parser_controls()) {
    std::fputs("gem5 parser controls failed\n", stderr);
    return 1;
  }
  if (!gem5_execution_controls(context)) {
    std::fputs("gem5 execution controls failed\n", stderr);
    return 1;
  }
  if (!checksum_controls() || !tma_controls() || !mca_controls(context)) {
    std::fputs("TMA/MCA controls failed\n", stderr);
    return 1;
  }
#if defined(__linux__)
  std::array<unsigned long, 1024> before{}, after{};
  if (::sched_getaffinity(0, sizeof(before), reinterpret_cast<cpu_set_t*>(before.data())) != 0) return 1;
#endif
  const auto hardware = pmu_gate(context);
#if defined(__linux__)
  if (::sched_getaffinity(0, sizeof(after), reinterpret_cast<cpu_set_t*>(after.data())) != 0 || before != after) return 1;
#endif
  if ((hardware.status != Status::Pass && hardware.status != Status::NotRun) ||
      hardware.detail.find("checksum") == std::string::npos)
    return 1;
  if (!extent_and_abi_controls() || !object_controls(context)) {
    std::fputs("extent/native ABI/exception control failed\n", stderr);
    return 1;
  }
  bool actual_negative = false;
  for (const auto& object : context.objects) {
    auto dis = command({(context.tools / "llvm-objdump").string(), "-d", "--no-show-raw-insn", object});
    if (!dis.ok) return 1;
    auto functions = parse_assembly(dis.output);
    auto it = functions.find("flow_select_register_u64");
    if (it != functions.end()) {
      if (check_function(Gate{"flow_select_register_u64", Prefetchw}, it->second).empty()) return 1;
      actual_negative = true;
    }
  }
  if (!actual_negative) return 1;
  const auto results = execute(context, "all");
  for (const auto& result : results) {
    if (result.status != Status::Pass &&
        !((result.gate == "pmu-gate" || result.gate == "gem5-gate") && result.status == Status::NotRun)) {
      std::fprintf(stderr, "%s: %s\n", result.gate.c_str(), result.detail.c_str());
      return 1;
    }
  }
  auto missing = context;
  missing.objects.clear();
  if (asm_gate(missing).status != Status::Fail || size_gate(missing).status != Status::Fail ||
      abi_gate(missing).status != Status::Fail)
    return 1;
  if (json_escape("\"\n\\") != R"json(\"\u000a\\)json") return 1;
  std::fputs("parser negatives, actual ELF negative, and all real-object gates passed\n", stdout);
}
