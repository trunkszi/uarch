/** @file @brief Portable constant-evaluated reflection queries and static text promotion. */
#pragma once

#include <cstddef>
#include <cstdlib>
#include <meta>
#include <optional>
#include <string_view>

namespace uarch::reflect {
/** @brief Context for representation audits, including non-public members. */
[[nodiscard]] consteval std::meta::access_context unchecked_context() noexcept {
  return std::meta::access_context::unchecked();
}

/** @brief Count exact payload types without vendor-specific annotation filtering. */
template <class Annotation>
[[nodiscard]] consteval std::size_t annotation_count_of(std::meta::info entity) {
  std::size_t count = 0;
  for (auto note : std::meta::annotations_of(entity)) {
    if (std::meta::remove_cv(std::meta::type_of(std::meta::constant_of(note))) == ^^Annotation) ++count;
  }
  return count;
}

/** @brief Retrieve a unique typed annotation; missing or duplicate payloads return no value. */
template <class Annotation>
[[nodiscard]] consteval std::optional<Annotation> annotation_of(std::meta::info entity) {
  std::optional<Annotation> result;
  for (auto note : std::meta::annotations_of(entity)) {
    auto value = std::meta::constant_of(note);
    if (std::meta::remove_cv(std::meta::type_of(value)) == ^^Annotation) {
      if (result) return std::nullopt;
      result = std::meta::extract<Annotation>(value);
    }
  }
  return result;
}

/** @brief Byte offset of a byte-aligned member; invalid offsets fail constant evaluation. */
[[nodiscard]] consteval std::size_t byte_offset(std::meta::info member) noexcept {
  const auto offset = std::meta::offset_of(member);
  if (offset.bytes < 0 || offset.bits != 0) std::abort();
  return static_cast<std::size_t>(offset.bytes);
}

/** @brief Promote exactly this text, including embedded nulls, to immutable static storage. */
[[nodiscard]] consteval std::string_view freeze_string(std::string_view text) {
  return {std::define_static_string(text), text.size()};
}
}  // namespace uarch::reflect
