/** @file @brief Fail-closed capability issuance and host observation errors. */
#pragma once
#include <cstdint>
#include <system_error>
#include <type_traits>

namespace uarch::detect {
/** @brief Failure to establish authority, or a proved missing instruction. */
enum class DetectError : std::uint8_t { UnverifiedExecutionDomain, Unsupported };
/** @brief Missing evidence differs from evidence that cannot be parsed safely. */
enum class AuditError : std::uint8_t { Unavailable, Malformed };
[[nodiscard]] std::error_code make_error_code(DetectError error) noexcept;
[[nodiscard]] std::error_code make_error_code(AuditError error) noexcept;
}  // namespace uarch::detect

template <>
struct std::is_error_code_enum<uarch::detect::DetectError> : std::true_type {};
template <>
struct std::is_error_code_enum<uarch::detect::AuditError> : std::true_type {};
