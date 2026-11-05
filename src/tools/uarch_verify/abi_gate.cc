#include <cctype>
#include <regex>
#include <set>

#include "verify.hpp"

namespace uarch::verify {
std::string ir_signature(std::string_view ir, std::string_view symbol) {
  const std::string marker = "@" + std::string(symbol) + "(";
  auto at = ir.find(marker);
  while (at != std::string_view::npos) {
    auto begin = ir.rfind('\n', at);
    begin = begin == std::string_view::npos ? 0 : begin + 1;
    if (ir.substr(begin, 7) == "define ") {
      auto end = at + marker.size();
      unsigned depth = 1;
      for (; end < ir.size(); ++end) {
        if (ir[end] == '(') ++depth;
        if (ir[end] == ')' && --depth == 0) return std::string(ir.substr(begin, end - begin + 1));
      }
      return {};
    }
    at = ir.find(marker, at + marker.size());
  }
  return {};
}
bool check_signature(std::string_view signature, probes::AbiRule rule) {
  if (signature.empty()) return false;
  bool pointer = false;
  // Parameter SSA names (for example %ptr) are names, not pointer type tokens.
  for (std::size_t i = 0; i < signature.size();) {
    if (std::isalpha(static_cast<unsigned char>(signature[i])) || signature[i] == '_') {
      auto begin = i++;
      while (i < signature.size() && (std::isalnum(static_cast<unsigned char>(signature[i])) || signature[i] == '_')) ++i;
      if (signature.substr(begin, i - begin) == "ptr" &&
          (begin == 0 || (signature[begin - 1] != '%' && signature[begin - 1] != '@')))
        pointer = true;
    } else
      ++i;
  }
  const bool memory = pointer || signature.find("byval(") != std::string_view::npos;
  if (rule == probes::AbiRule::MemoryControl) return memory;
  if (rule == probes::AbiRule::Registers)
    return !memory && signature.find("sret(") == std::string_view::npos &&
           signature.find("dead_on_unwind") == std::string_view::npos && signature.find("x86_fp80") == std::string_view::npos;
  return true;
}
Result abi_gate(const Context& context) {
  if (context.objects.empty()) return {"abi-check", Status::Fail, "no native probe objects"};
  std::set<std::string> native_seen;
  std::string ir;
  for (auto file : {"layout_probes.cc", "arithmetic_probes.cc"}) {
    auto result =
        command({(context.tools / "clang++").string(), "-std=c++26", "-freflection-latest", "-stdlib=libc++", "-fno-exceptions",
                 "-fno-rtti", "-O3", "-fno-lto", "-S", "-emit-llvm", "-I" + (context.root / "include").string(),
                 (context.root / "src/probes" / file).string(), "-o", "-"});
    if (!result.ok) return {"abi-check", Status::Fail, result.output};
    ir += result.output;
  }
  std::string errors;
  for (const auto& gate : probes::gates)
    if (gate.abi != probes::AbiRule::None) {
      auto signature = ir_signature(ir, gate.symbol);
      if (!check_signature(signature, gate.abi))
        errors += "IR ABI mismatch " + std::string(gate.symbol) + ": " + signature + '\n';
    }
  for (const auto& object : context.objects) {
    auto inspected = inspect_object(context, object);
    if (!inspected.ok) return {"abi-check", Status::Fail, inspected.error};
    auto& functions = inspected.functions;
    for (const auto& gate : probes::gates)
      if (gate.abi == probes::AbiRule::Registers) {
        auto it = functions.find(std::string(gate.symbol));
        if (it != functions.end()) native_seen.insert(std::string(gate.symbol));
        if (it != functions.end() && (stack_parameter(it->second) || (!gate.pointer_args && indirect_parameter(it->second))))
          errors += "native stack/indirect value parameter " + std::string(gate.symbol) + '\n';
      }
  }
  for (const auto& gate : probes::gates)
    if (gate.abi == probes::AbiRule::Registers && !native_seen.contains(std::string(gate.symbol)))
      errors += "missing native ABI symbol " + std::string(gate.symbol) + '\n';
  const std::string shapes = R"(
struct RegisterPair {unsigned long long a,b;};
struct FloatPair {double a,b;};
struct alignas(16) OverAlignedFloat {float a;};
struct Empty {};
struct EmptyAndDouble {Empty e;double d;};
struct NuaFloatPair {[[no_unique_address]] Empty e;float a,b;};
)";
  const std::string predictions = R"(
#include <uarch/core/layout/abi_class.hpp>
using namespace uarch::layout;
static_assert(aapcs64_classify<RegisterPair>()==AbiShape{AbiClass::Integer,AbiClass::Integer,2});
static_assert(aapcs64_classify<FloatPair>()==AbiShape{AbiClass::Sse,AbiClass::Sse,2});
static_assert(aapcs64_classify<OverAlignedFloat>()==AbiShape{AbiClass::Integer,AbiClass::Integer,2});
static_assert(aapcs64_classify<EmptyAndDouble>()==AbiShape{AbiClass::Integer,AbiClass::Integer,2});
static_assert(aapcs64_classify<NuaFloatPair>()==AbiShape{AbiClass::Sse,AbiClass::Sse,2});
)";
  auto predicted =
      command({(context.tools / "clang++").string(), "-std=c++26", "-freflection-latest", "-stdlib=libc++", "-fno-exceptions",
               "-fno-rtti", "-O3", "-S", "-emit-llvm", "-I" + (context.root / "include").string(), "-x", "c++", "-", "-o", "-"},
              shapes + predictions);
  if (!predicted.ok) return {"abi-check", Status::Fail, "native AAPCS64 classifier checks failed: " + predicted.output};
  const std::string source = shapes + R"(
extern "C" RegisterPair aapcs_gpr(RegisterPair x){x.a+=1;return x;}
extern "C" FloatPair aapcs_hfa(FloatPair x){x.a+=1;return x;}
extern "C" OverAlignedFloat aapcs_over(OverAlignedFloat x){x.a+=1;return x;}
extern "C" EmptyAndDouble aapcs_empty(EmptyAndDouble x){x.d+=1;return x;}
extern "C" NuaFloatPair aapcs_nua(NuaFloatPair x){x.a+=1;return x;}
)";
  auto foreign = command({(context.tools / "clang++").string(), "--target=aarch64-linux-gnu", "-std=c++26", "-ffreestanding",
                          "-fno-exceptions", "-fno-rtti", "-O3", "-S", "-emit-llvm", "-x", "c++", "-", "-o", "-"},
                         source);
  if (!foreign.ok) return {"abi-check", Status::Fail, "AAPCS64 compile failed: " + foreign.output};
  const auto gpr = ir_signature(foreign.output, "aapcs_gpr"), hfa = ir_signature(foreign.output, "aapcs_hfa");
  if (gpr.find("[2 x i64]") == std::string::npos || hfa.find("[2 x double]") == std::string::npos ||
      !check_signature(gpr, probes::AbiRule::Registers) || !check_signature(hfa, probes::AbiRule::Registers))
    errors += "AAPCS64 HFA/GPR lowering disagrees with classifier\n";
  const auto nua = ir_signature(foreign.output, "aapcs_nua");
  if (nua.find("[2 x float]") == std::string::npos || !check_signature(nua, probes::AbiRule::Registers))
    errors += "AAPCS64 no-unique-address HFA lowering disagrees with classifier\n";
  for (auto name : {"aapcs_over", "aapcs_empty"}) {
    const auto signature = ir_signature(foreign.output, name);
    if (signature.find("[2 x i64]") == std::string::npos || signature.find("[1 x float]") != std::string::npos ||
        signature.find("[2 x double]") != std::string::npos || !check_signature(signature, probes::AbiRule::Registers))
      errors += "AAPCS64 non-HFA incorrectly lowered: " + std::string(name) + '\n';
  }
  return {
      "abi-check", errors.empty() ? Status::Pass : Status::Fail,
      errors.empty() ? "Clang IR plus active-compiler native ABI; AAPCS64 five-shape lowering and classifier agree" : errors};
}
}  // namespace uarch::verify
