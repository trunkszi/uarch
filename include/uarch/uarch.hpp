/** @file @brief Hosted facade including explicit execution-domain detection. */
#pragma once
#include <uarch/detect/detect.hpp>
#include <uarch/detect/error.hpp>
#include <uarch/detect/host.hpp>
#include <uarch/detect/policy.hpp>
#include <uarch/primitives.hpp>
namespace uarch {
using detect::DetectError;
using detect::DetectPolicy;
using detect::ExecutionDomainPolicy;
using detect::UnknownPdep;
using detect::WidePolicy;
namespace prelude {
using uarch::DetectError;
using uarch::DetectPolicy;
using uarch::ExecutionDomainPolicy;
using uarch::UnknownPdep;
using uarch::WidePolicy;
using uarch::detect::detect;
using uarch::detect::detect_prefetch_write;
}  // namespace prelude
}  // namespace uarch
