#include <algorithm>
#include <regex>

#include "verify.hpp"

namespace uarch::verify {
namespace {
std::string splice_lines(std::string_view text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      if (text[i + 1] == '\n') {
        ++i;
        continue;
      }
      if (text[i + 1] == '\r' && i + 2 < text.size() && text[i + 2] == '\n') {
        i += 2;
        continue;
      }
    }
    out += text[i];
  }
  return out;
}
std::string code_only(std::string_view text) {
  std::string out(text);
  auto erase = [&](std::size_t begin, std::size_t end) {
    for (auto i = begin; i < end; ++i)
      if (out[i] != '\n') out[i] = ' ';
  };
  const auto number_char = [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
  };
  bool in_number = false;
  for (std::size_t i = 0; i < text.size();) {
    if (text[i] >= '0' && text[i] <= '9' && (i == 0 || !number_char(text[i - 1])))
      in_number = true;
    else if (!number_char(text[i]) && text[i] != '\'')
      in_number = false;
    auto start = i;
    if (text.substr(i, 2) == "//") {
      auto end = text.find('\n', i + 2);
      if (end == std::string_view::npos) end = text.size();
      erase(i, end);
      i = end;
    } else if (text.substr(i, 2) == "/*") {
      auto end = text.find("*/", i + 2);
      end = end == std::string_view::npos ? text.size() : end + 2;
      erase(i, end);
      i = end;
    } else if (text.substr(i, 2) == "R\"") {
      auto open = text.find('(', i + 2);
      if (open == std::string_view::npos || open - i > 18) {
        ++i;
        continue;
      }
      auto close = ")" + std::string(text.substr(i + 2, open - i - 2)) + "\"";
      auto end = text.find(close, open + 1);
      end = end == std::string_view::npos ? text.size() : end + close.size();
      erase(i, end);
      i = end;
    } else if (text[i] == '"' || (text[i] == '\'' && !in_number)) {
      const char quote = text[i++];
      while (i < text.size()) {
        if (text[i] == '\\') {
          i = std::min(i + 2, text.size());
          continue;
        }
        if (text[i++] == quote) break;
      }
      erase(start, i);
    } else
      ++i;
  }
  return out;
}
bool has(const std::string& text, const char* pattern) { return std::regex_search(text, std::regex(pattern)); }
}  // namespace
std::vector<std::string> lint_text(std::string_view path, std::string_view text) {
  std::vector<std::string> errors;
  const auto lines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) +
                     static_cast<std::size_t>(!text.empty() && !text.ends_with('\n'));
  if (lines > 800) errors.emplace_back("file exceeds 800 lines");
  if (path != "README.zh-CN.md" && std::ranges::any_of(text, [](unsigned char c) { return c > 127; }))
    errors.emplace_back("non-ASCII source");
  const auto spliced = splice_lines(text);
  const auto code = code_only(spliced);
  const bool include = path.starts_with("include/");
  if ((include || path.starts_with("src/")) && has(code, R"((^|\n)\s*#\s*define\b)"))
    errors.emplace_back("source macro definition");
  if (include) {
    if (has(code, R"((\.|->)\s*value\s*\(\s*\))")) errors.emplace_back("exception-capable value access");
    if (has(code, R"(\bstd\s*::\s*function\b)")) errors.emplace_back("heap-capable callback");
    if (has(code, R"(#\s*include\s*<\s*iostream\s*>)")) errors.emplace_back("iostream in public header");
    if (has(code, R"(\b(throw|try|catch)\b)")) errors.emplace_back("exception syntax in public header");
  }
  if (!path.starts_with("include/uarch/arch/") && !path.starts_with("src/detect/") && has(code, R"(\bTokenFactory\b)"))
    errors.emplace_back("unauthorized token factory");
  const bool direct_unchecked =
      path != "include/uarch/core/reflect.hpp" && has(code, R"(\baccess_context\s*::\s*unchecked\b|\bunchecked\s*\()");
  const bool unchecked_wrapper = !path.starts_with("include/uarch/core/") && path != "tests/unit/core/reflect_test.cc" &&
                                 has(code, R"(\bunchecked_context\b)");
  if (direct_unchecked || unchecked_wrapper) errors.emplace_back("unauthorized unchecked reflection");
  if (has(code,
          R"(reinterpret_cast\s*<[^>]*(\bScalar\b|\bX86V3\b|\bX86V3SlowPdep\b|\bX86V4\b|\bAarch64Neon\b|\bPrefetchWrite\b|\bTier\b))"))
    errors.emplace_back("capability reinterpretation");
  return errors;
}
Result lint_gate(const Context& context) {
  std::string errors;
  std::size_t count = 0;
  for (auto directory : {"include", "src", "tests", "benches"}) {
    std::error_code ec;
    const auto base = context.root / directory;
    if (!std::filesystem::is_directory(base, ec) || ec)
      return {"lint", Status::Fail, "missing source directory " + base.string()};
    std::filesystem::recursive_directory_iterator it(base, ec), end;
    if (ec) return {"lint", Status::Fail, ec.message()};
    while (it != end) {
      const auto path = it->path();
      if (it->is_regular_file(ec)) {
        auto relative = path.lexically_relative(context.root).generic_string();
        for (const auto& error : lint_text(relative, read(path))) {
          errors.append(relative).append(": ").append(error).push_back('\n');
        }
        ++count;
      }
      if (ec) return {"lint", Status::Fail, ec.message()};
      it.increment(ec);
      if (ec) return {"lint", Status::Fail, ec.message()};
    }
  }
  return {"lint", errors.empty() ? Status::Pass : Status::Fail,
          errors.empty() ? "checked " + std::to_string(count) + " files" : errors};
}
}  // namespace uarch::verify
