#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <uarch_verify_config.hpp>

#include "verify.hpp"

namespace uarch::verify {
std::vector<std::string> mca_cpu_models(const Context& context) {
  auto sources = context.mca_cpus;
  if (sources.empty()) {
    if (const auto* env = std::getenv("UARCH_MCA_CPU"))
      sources.emplace_back(env);
    else {
#if defined(__aarch64__)
      sources = {"neoverse-v2"};
#else
      sources = {"znver4", "znver5", "sapphirerapids"};
#endif
    }
  }
  std::vector<std::string> models;
  for (const auto& source : sources) {
    const auto source_begin = source.find_first_not_of(" \t");
    if (source_begin == std::string_view::npos) return {};
    const auto raw = source.substr(source_begin, source.find_last_not_of(" \t") - source_begin + 1);
    if (raw.empty() || raw.front() == ',' || raw.back() == ',') return {};
    std::istringstream words(raw);
    std::string model;
    while (std::getline(words, model, ',')) {
      const auto begin = model.find_first_not_of(" \t");
      if (begin == std::string_view::npos) return {};
      model = model.substr(begin, model.find_last_not_of(" \t") - begin + 1);
      if (model.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string_view::npos)
        return {};
      if (std::ranges::find(models, model) == models.end()) models.push_back(model);
    }
  }
  return models;
}
namespace {
std::optional<double> mca_metric(const std::string& text, std::string_view key) {
  const auto start = text.find(std::string(key) + ":");
  if (start == std::string_view::npos) return std::nullopt;
  const auto begin = text.find_first_not_of(" \t", start + key.size() + 1);
  if (begin == std::string_view::npos) return std::nullopt;
  double value = 0;
  auto result = std::from_chars(text.data() + begin, text.data() + text.size(), value);
  if (result.ec != std::errc{} || !std::isfinite(value) || value < 0) return std::nullopt;
  return value;
}
std::string mca_assembly(const std::string& disassembly, std::string_view symbol, const Function& function) {
  std::istringstream lines(disassembly);
  std::string line, assembly;
  bool active = false;
  std::size_t emitted = 0;
  const std::regex instruction(R"(^\s*([0-9a-fA-F]+):\s+(\S+)\s*(.*?)\s*$)");
  const std::regex target(R"((?:0x)?([0-9a-fA-F]+)\s*<[^>]+>)");
  while (std::getline(lines, line)) {
    if (line.find("<" + std::string(symbol) + ">:") != std::string_view::npos) {
      active = true;
      continue;
    }
    if (!active || emitted >= function.instructions.size()) continue;
    std::smatch match;
    if (!std::regex_match(line, match, instruction)) continue;
    auto operands = match[3].str();
    operands = operands.substr(0, operands.find('#'));
    operands = std::regex_replace(operands, target, ".Lmca_$1");
    assembly += ".Lmca_" + match[1].str() + ":\n\t" + match[2].str() + " " + operands + "\n";
    ++emitted;
  }
  return emitted == function.instructions.size() ? assembly : std::string{};
}
}  // namespace
Result mca_gate(const Context& context) {
  const auto cpus = mca_cpu_models(context);
  if (cpus.empty()) return {"mca-gate", Status::Fail, "empty/invalid MCA CPU list"};
#if defined(__aarch64__)
  const std::string triple = "aarch64-unknown-linux-gnu";
#else
  const std::string triple = "x86_64-unknown-linux-gnu";
#endif
  std::map<std::string, std::pair<Function, std::string>> functions;
  for (const auto& object : context.objects) {
    const auto inspected = inspect_object(context, object);
    if (!inspected.ok) return {"mca-gate", Status::Fail, inspected.error};
    auto dis = command({(context.tools / "llvm-objdump").string(), "-d", "--no-show-raw-insn", object});
    if (!dis.ok) return {"mca-gate", Status::Fail, dis.output};
    for (const auto& [name, function] : inspected.functions) {
      if (std::ranges::none_of(probes::gates, [&](const auto& gate) { return gate.max_throughput > 0 && gate.symbol == name; }))
        continue;
      if (!functions.emplace(name, std::pair{function, mca_assembly(dis.output, name, function)}).second)
        return {"mca-gate", Status::Fail, "duplicate symbol " + name};
    }
  }
  std::string report;
  bool passed = true;
  std::size_t budgeted = 0;
  for (const auto& gate : probes::gates) {
    if (gate.max_throughput <= 0) continue;
    ++budgeted;
    const auto found = functions.find(std::string(gate.symbol));
    if (found == functions.end() || found->second.second.empty())
      return {"mca-gate", Status::Fail, "budgeted symbol missing: " + std::string(gate.symbol)};
    if (!check_function(gate, found->second.first).empty())
      return {"mca-gate", Status::Fail, "assembly contract failed: " + std::string(gate.symbol)};
    for (const auto& cpu : cpus) {
      const auto result =
          command({(context.tools / "llvm-mca").string(), "--bottleneck-analysis", "-mtriple=" + triple, "-mcpu=" + cpu},
                  found->second.second);
      if (!result.ok) return {"mca-gate", Status::Fail, std::string(gate.symbol) + "/" + cpu + ": " + result.output};
      const auto throughput = mca_metric(result.output, "Block RThroughput");
      const auto uops = mca_metric(result.output, "Total uOps"), width = mca_metric(result.output, "Dispatch Width");
      if (!throughput || !uops || !width) return {"mca-gate", Status::Fail, "missing MCA metrics: " + result.output};
      const auto limit = gate.max_throughput * (cpu == gate.mca_cpu ? 1.0 : 1.25);
      passed &= *throughput <= limit;
      report += std::string(gate.symbol) + " cpu=" + cpu + " Block RThroughput=" + std::to_string(*throughput) +
                " budget=" + std::to_string(limit) + " Total uOps=" + std::to_string(*uops) +
                " Dispatch Width=" + std::to_string(*width) + "\n";
      std::istringstream lines(result.output);
      std::string line;
      std::size_t indicators = 0;
      while (std::getline(lines, line))
        if (indicators < 5 &&
            (line.find("bottleneck") != std::string_view::npos || line.find("Pressure") != std::string_view::npos ||
             line.find("pressure") != std::string_view::npos || line.find("Dependencies") != std::string_view::npos)) {
          report += "  " + line + "\n";
          ++indicators;
        }
    }
  }
  if (!budgeted) return {"mca-gate", Status::Fail, "no budgeted MCA probes"};
  return {"mca-gate", passed ? Status::Pass : Status::Fail, report};
}
bool exception_artifacts(std::string_view text) {
  constexpr std::array artifacts{".gcc_except_table",    "__cxa_throw",      "__cxa_rethrow",
                                 "__cxa_begin_catch",    "__cxa_end_catch",  "__cxa_allocate_exception",
                                 "__cxa_free_exception", "__gxx_personality"};
  return std::ranges::any_of(artifacts, [&](auto artifact) { return text.find(artifact) != std::string_view::npos; });
}
Result size_gate(const Context& context) {
  if (context.objects.empty()) return {"size-report", Status::Fail, "no probe objects"};
  std::map<std::string, std::uint64_t> extents;
  std::string report;
  for (const auto& object : context.objects) {
    auto bytes = read(object);
    if (bytes.size() < 4 || bytes.substr(0, 4) != std::string("\177ELF", 4))
      return {"size-report", Status::Fail, "probe is not native ELF: " + object};
    auto tables = command({(context.tools / "llvm-readobj").string(), "--sections", "--symbols", object});
    if (!tables.ok) return {"size-report", Status::Fail, tables.output};
    if (exception_artifacts(tables.output)) return {"size-report", Status::Fail, "exception artifact in " + object};
    auto nm = command(
        {(context.tools / "llvm-nm").string(), "--defined-only", "--print-size", "--size-sort", "--format=posix", object});
    auto size = command({(context.tools / "llvm-size").string(), object});
    if (!nm.ok || !size.ok) return {"size-report", Status::Fail, nm.output + size.output};
    std::istringstream lines(nm.output);
    std::string line;
    while (std::getline(lines, line)) {
      std::istringstream words(line);
      std::string name, type, address, extent;
      words >> name >> type >> address >> extent;
      if (type != "T" && type != "W" && type != "t") continue;
      std::uint64_t value = 0;
      auto result = std::from_chars(extent.data(), extent.data() + extent.size(), value, 16);
      if (result.ec != std::errc{} || result.ptr != extent.data() + extent.size())
        return {"size-report", Status::Fail, "invalid symbol size"};
      if (!extents.emplace(name, value).second && !name.starts_with("_Z"))
        return {"size-report", Status::Fail, "duplicate exported symbol " + name};
    }
    report += size.output + nm.output;
  }
  for (const auto& gate : probes::gates) {
    auto found = extents.find(std::string(gate.symbol));
    if (found == extents.end() || found->second == 0)
      return {"size-report", Status::Fail, "missing/zero extent " + std::string(gate.symbol)};
  }
  return {"size-report", Status::Pass, report};
}
std::string json_escape(std::string_view text) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (char character : text) {
    const auto c = static_cast<unsigned char>(character);
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += R"json(\\)json";
    else if (c < 32) {
      out += "\\u00";
      out += digits[c >> 4];
      out += digits[c & 15];
    } else
      out += static_cast<char>(c);
  }
  return out;
}
std::string status_name(Status state) {
  switch (state) {
    case Status::Pass:
      return "Pass";
    case Status::Fail:
      return "Fail";
    case Status::NotRun:
      return "NotRun";
    case Status::Unsupported:
      return "Unsupported";
  }
  return "Fail";
}
std::vector<Result> execute(const Context& context, std::string_view name) {
  std::vector<Result> results;
  bool known = name == "all";
  const std::pair<std::string_view, Result (*)(const Context&)> gates[] = {
      {"lint", lint_gate},        {"asm-check", asm_gate}, {"abi-check", abi_gate}, {"mca-gate", mca_gate},
      {"size-report", size_gate}, {"pmu-gate", pmu_gate},  {"gem5-gate", gem5_gate}};
  for (const auto& [gate, run] : gates) {
    if (name == "all" || name == gate) {
      known = true;
      results.push_back(run(context));
    } else
      results.push_back({std::string(gate), Status::NotRun, "not requested"});
  }
  if (!known) results.push_back({std::string(name), Status::Fail, "unknown gate"});
  return results;
}
std::string report_json(const Context& context, const std::vector<Result>& results) {
  auto version = command({(context.tools / "llvm-objdump").string(), "--version"});
  std::vector<std::string> hash_args{"/usr/bin/sha256sum"};
  for (auto name : {"layout", "bits", "flow", "arithmetic", "mem", "arch"})
    hash_args.push_back((context.root / "src/probes" / (std::string(name) + "_probes.cc")).string());
  hash_args.push_back((context.root / "src/probes/delegate_targets.cc").string());
  hash_args.push_back((context.root / "include/uarch/probes/delegate_targets.hpp").string());
  hash_args.push_back((context.root / "include/uarch/core/flow/delegate.hpp").string());
  hash_args.push_back((context.root / "include/uarch/probes/gate.hpp").string());
  auto hashes = command(hash_args);
  std::string json = R"json({"compiler":")json" + json_escape(config::compiler) + R"json(","probe_flags":")json" +
                     json_escape(config::flags) + R"json(","llvm":")json" +
                     json_escape(version.output.substr(0, version.output.find('\n'))) +
                     R"json(","source_sha256_manifest":")json" + json_escape(hashes.output) + R"json(","source_root":")json" +
                     json_escape(context.root.string()) + R"json(","objects":[)json";
  for (std::size_t i = 0; i < context.objects.size(); ++i) {
    if (i) json += ',';
    json += '"' + json_escape(context.objects[i]) + '"';
  }
  json += R"json(],"gates":[)json";
  for (std::size_t i = 0; i < results.size(); ++i) {
    if (i) json += ',';
    json += R"json({"gate":")json" + json_escape(results[i].gate) + R"json(","status":")json" + status_name(results[i].status) +
            R"json(","detail":")json" + json_escape(results[i].detail) + R"json("})json";
  }
  return json + "]}\n";
}
}  // namespace uarch::verify
