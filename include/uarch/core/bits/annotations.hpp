/** @file @brief Structural single-bit and low/width field annotations. */
#pragma once

#include <cstdint>

namespace uarch::bits {
/** @brief One logical bit at the given zero-based index. */
struct Flag {
  std::uint8_t bit; /**< Index within the backing word. */
};
/** @brief A contiguous unsigned field; width counts bits rather than an inclusive high index. */
struct Field {
  std::uint8_t low;   /**< Least significant bit index. */
  std::uint8_t width; /**< Number of bits, including the low bit. */
};
}  // namespace uarch::bits
