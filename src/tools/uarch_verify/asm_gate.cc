#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <uarch_verify_config.hpp>

#include "verify.hpp"

namespace uarch::verify {
std::string temporary_pattern(std::string_view name) {
  const auto* configured = std::getenv("TMPDIR");
  const auto base =
      configured && *configured ? std::filesystem::path(configured) : std::filesystem::path(config::root) / "build/tmp";
  std::error_code error;
  std::filesystem::create_directories(base, error);
  if (error) return {};
  return (base / (std::string(name) + "-XXXXXX")).string();
}
std::string read(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
std::string shell_quote(std::string_view text) {
  std::string out = "'";
  for (char c : text) out += c == '\'' ? "'\\''" : std::string(1, c);
  return out + "'";
}
Command command(const std::vector<std::string>& args, std::string_view input) {
  if (args.empty()) return {false, "empty command"};
  std::string pattern = temporary_pattern("uarch-verify");
  auto* made = ::mkdtemp(pattern.data());
  if (!made) return {false, "cannot create temporary command directory"};
  const std::filesystem::path dir(made);
  const auto in = dir / "input", out = dir / "output";
  {
    std::ofstream file(in);
    file << input;
    if (!file) {
      std::error_code ec;
      std::filesystem::remove_all(dir, ec);
      return {false, "cannot write command input"};
    }
  }
  std::string cmd;
  for (const auto& arg : args) {
    if (!cmd.empty()) cmd += ' ';
    cmd += shell_quote(arg);
  }
  cmd += " < " + shell_quote(in.string()) + " > " + shell_quote(out.string()) + " 2>&1";
  const int status = std::system(cmd.c_str());
  Command result{status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0, read(out)};
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  return result;
}
Context default_context() {
  Context context{std::filesystem::path(config::root), {}, std::filesystem::path(config::tools)};
  std::istringstream lines{std::string(config::objects)};
  std::string line;
  while (std::getline(lines, line))
    if (!line.empty()) context.objects.push_back(line);
  return context;
}
Functions parse_assembly(std::string_view input, const SymbolExtents& extents) {
  Functions functions;
  std::string section;
  Function* current = nullptr;
  std::istringstream lines{std::string(input)};
  std::string line;
  const std::regex symbol(R"(^\s*([0-9a-fA-F]+) <([^>]+)>:$)");
  const std::regex instruction(R"(^\s*([0-9a-fA-F]+):\s+([A-Za-z][A-Za-z0-9_.]*)\s*(.*)$)");
  while (std::getline(lines, line)) {
    if (line.starts_with("Disassembly of section ")) {
      section = line.substr(23);
      if (section.ends_with(':')) section.pop_back();
      current = nullptr;
      continue;
    }
    std::smatch match;
    if (std::regex_match(line, match, symbol)) {
      auto name = match[2].str();
      if (!extents.empty() && (!extents.contains(name) || extents.at(name).bytes == 0)) {
        current = nullptr;
        continue;
      }
      current = &functions[name];
      *current = Function{name, section, {}, {}};
      auto address = match[1].str();
      std::from_chars(address.data(), address.data() + address.size(), current->base_addr, 16);
      if (!extents.empty()) {
        current->base_addr = extents.at(name).address;
        current->max_bytes = extents.at(name).bytes;
      }
      continue;
    }
    if (current && std::regex_match(line, match, instruction)) {
      auto address_text = match[1].str();
      std::uint64_t address = 0;
      std::from_chars(address_text.data(), address_text.data() + address_text.size(), address, 16);
      if (current->max_bytes && (address < current->base_addr || address - current->base_addr >= current->max_bytes)) {
        current = nullptr;
        continue;
      }
      auto mnemonic = match[2].str();
      if (mnemonic.starts_with("R_")) {
        current->references += line + '\n';
        if (!current->instructions.empty()) current->instructions.back().operands += " " + line;
        continue;
      }
      std::ranges::transform(mnemonic, mnemonic.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      current->instructions.push_back({mnemonic, match[3].str()});
    }
  }
  return functions;
}
SymbolExtents parse_symbol_extents(std::string_view text) {
  SymbolExtents result;
  std::istringstream lines{std::string(text)};
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    std::string name, type, address, size;
    fields >> name >> type >> address >> size;
    if (type != "T" && type != "t" && type != "W" && type != "w") continue;
    SymbolExtent extent;
    const auto a = std::from_chars(address.data(), address.data() + address.size(), extent.address, 16);
    const auto n = std::from_chars(size.data(), size.data() + size.size(), extent.bytes, 16);
    if (a.ec == std::errc{} && a.ptr == address.data() + address.size() && n.ec == std::errc{} &&
        n.ptr == size.data() + size.size())
      result.emplace(name, extent);
  }
  return result;
}
ObjectAssembly inspect_object(const Context& context, const std::string& object) {
  auto symbols = command({(context.tools / "llvm-nm").string(), "-S", "--defined-only", "--format=posix", object});
  auto dis = command({(context.tools / "llvm-objdump").string(), "-dr", "--no-show-raw-insn", object});
  if (!symbols.ok || !dis.ok) return {false, {}, symbols.output + dis.output};
  auto extents = parse_symbol_extents(symbols.output);
  if (extents.empty()) return {false, {}, "missing native function extents: " + object};
  return {true, parse_assembly(dis.output, extents), {}};
}
namespace {
bool external_branch(const Instruction& instruction, std::string_view current) {
  const auto& operand = instruction.operands;
  if (operand.find('*') != std::string::npos || operand.find("@plt") != std::string::npos ||
      operand.find("R_") != std::string::npos)
    return true;
  std::smatch match;
  std::string target;
  const bool named = std::regex_search(operand, match, std::regex(R"(<([^>]+)>)"));
  if (named)
    target = match[1].str();
  else {
    std::istringstream tokens(operand);
    tokens >> target;
  }
  if (target.empty() || target.starts_with(".L")) return false;
  auto base = target.substr(0, target.find_first_of("+-"));
  if (base == current) return false;
  if (target.size() > 1 && (target.back() == 'f' || target.back() == 'b') &&
      std::ranges::all_of(target.substr(0, target.size() - 1), [](unsigned char c) { return std::isdigit(c); }))
    return false;
  if (target.starts_with("0x")) target = target.substr(2);
  return named || !std::ranges::all_of(target, [](unsigned char c) { return std::isxdigit(c); });
}
}  // namespace
std::uint32_t classify(const Instruction& i, std::string_view current_symbol) {
  using namespace probes;
  const auto& m = i.mnemonic;
  std::uint32_t flags = 0;
  if ((m.starts_with('j') && !m.starts_with("jmp")) || m.starts_with("b.") || m == "cbz" || m == "cbnz" || m == "tbz" ||
      m == "tbnz")
    flags |= Jcc;
  if (m.starts_with("movabs")) flags |= Movabs;
  if (m.starts_with("call") || m == "bl" || m == "blr" || m == "br" ||
      ((m.starts_with("jmp") || m == "b") && external_branch(i, current_symbol)))
    flags |= Call;
  if (m.starts_with("call") || m == "bl" || m == "blr") flags |= DirectCall;
  if (m == "br" || ((m.starts_with("jmp") || m == "b") && external_branch(i, current_symbol))) flags |= TailJmp;
  if (m.starts_with("tzcnt")) flags |= Tzcnt;
  if (m.starts_with("blsr")) flags |= Blsr;
  if (m.starts_with("cmov")) flags |= Cmov;
  if (m == "div" || m == "divq" || m == "divl" || m == "divw" || m == "divb" || m == "udiv" || m == "sdiv") flags |= Div;
  if (m == "idiv" || m == "idivq" || m == "idivl" || m == "idivw" || m == "idivb") flags |= Idiv;
  if (i.operands.find("__udivti3") != std::string::npos) flags |= Udivti3;
  if (m.starts_with("pext")) flags |= Pext;
  if (m.starts_with("pdep")) flags |= Pdep;
  if (m == "prefetchw") flags |= Prefetchw;
  if (m == "prefetcht0") flags |= PrefetchT0;
  if (i.operands.find("%rsp") != std::string::npos || i.operands.find("%rbp") != std::string::npos ||
      std::regex_search(i.operands, std::regex(R"(\[\s*(sp|x29)\b)")) || m.starts_with("push") || m.starts_with("pop") ||
      m == "enter" || m.starts_with("leave"))
    flags |= Stack;
  if (m.starts_with("kmov") || std::regex_search(i.operands, std::regex(R"(%[xyz]mm[0-9]|\bv[0-9]+\.)"))) flags |= Simd;
  if (m.starts_with("vpcmp") || m.starts_with("pcmp") || m.starts_with("vcmp") || m.starts_with("cmgt") ||
      m.starts_with("cmge") || m == "cmp")
    flags |= Compare;
  return flags;
}
std::vector<std::string> check_function(const probes::Gate& gate, const Function& function) {
  std::vector<std::string> errors;
  if (function.symbol != gate.symbol || function.instructions.empty()) {
    errors.push_back("missing or empty symbol " + std::string(gate.symbol));
    return errors;
  }
  std::uint32_t flags = 0;
  for (const auto& i : function.instructions) flags |= classify(i, function.symbol);
  if (function.references.find("__udivti3") != std::string::npos) flags |= probes::Udivti3;
  if ((flags & gate.require) != gate.require) errors.push_back("missing required opcode: " + std::string(gate.symbol));
  if ((flags & gate.deny) != 0) errors.push_back("forbidden opcode: " + std::string(gate.symbol));
  if ((gate.require & probes::TailJmp) != 0) {
    const auto tails = std::ranges::count_if(function.instructions, [&](const auto& instruction) {
      return (classify(instruction, function.symbol) & probes::TailJmp) != 0;
    });
    if (tails != 1) errors.push_back("expected exactly one tail transfer: " + std::string(gate.symbol));
  }
  if (gate.max_instructions && function.instructions.size() > gate.max_instructions)
    errors.push_back("instruction budget exceeded: " + std::string(gate.symbol));
  if (!gate.section.empty() && !function.section.starts_with(gate.section))
    errors.push_back("wrong section: " + std::string(gate.symbol));
  return errors;
}
bool stack_parameter(const Function& function) {
  const std::regex stack(R"(^\s*(0x[0-9a-fA-F]+|[1-9][0-9]*)\(%rsp(?:,[^)]*)?\)|\(%rbp(?:,[^)]*)?\)|\[\s*(sp|x29)\b)");
  for (const auto& i : function.instructions) {
    if (i.mnemonic.starts_with("lea")) continue;
    std::size_t comma = 0;
    unsigned depth = 0;
    for (; comma < i.operands.size(); ++comma) {
      const char c = i.operands[comma];
      if (c == '(') ++depth;
      if (c == ')' && depth) --depth;
      if (c == ',' && !depth) break;
    }
    if (std::regex_search(i.operands.substr(0, comma), stack)) return true;
  }
  return false;
}
bool indirect_parameter(const Function& function) {
  const std::regex argument(R"(\([^)]*%(rdi|rsi|rdx|rcx|r8|r9)\b[^)]*\)|\[\s*x[0-3]\b)");
  return std::ranges::any_of(function.instructions, [&](const auto& i) {
    return !i.mnemonic.starts_with("lea") && std::regex_search(i.operands, argument);
  });
}
Result asm_gate(const Context& context) {
  if (context.objects.empty()) return {"asm-check", Status::Fail, "no probe objects"};
  Functions all;
  for (const auto& object : context.objects) {
    auto inspected = inspect_object(context, object);
    auto symbols = command({(context.tools / "llvm-readobj").string(), "--sections", "--symbols", object});
    if (!inspected.ok || !symbols.ok) return {"asm-check", Status::Fail, inspected.error + symbols.output};
    auto& parsed = inspected.functions;
    for (auto& [name, function] : parsed) {
      if (all.contains(name)) return {"asm-check", Status::Fail, "duplicate symbol " + name};
      all.emplace(name, std::move(function));
    }
    // Section ownership is checked from readobj's symbol records as well as disassembly.
    for (const auto& gate : probes::gates)
      if (!gate.section.empty()) {
        const auto name = "Name: " + std::string(gate.symbol) + " (";
        auto at = symbols.output.find(name);
        if (at != std::string::npos) {
          auto end = symbols.output.find('}', at);
          auto record = symbols.output.substr(at, end - at);
          if (record.find("Section: " + std::string(gate.section)) == std::string::npos)
            return {"asm-check", Status::Fail, "readobj cold section mismatch " + std::string(gate.symbol)};
        }
      }
  }
  std::string errors;
  for (const auto& gate : probes::gates) {
    auto found = all.find(std::string(gate.symbol));
    if (found == all.end()) {
      errors += "missing symbol " + std::string(gate.symbol) + '\n';
      continue;
    }
    for (auto& error : check_function(gate, found->second)) errors += error + '\n';
  }
  return {"asm-check", errors.empty() ? Status::Pass : Status::Fail,
          errors.empty() ? "all concrete probe symbols and opcode/section budgets passed" : errors};
}
}  // namespace uarch::verify
