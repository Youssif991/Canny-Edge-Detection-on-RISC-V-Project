/**
 * @file    gaussian.hpp
 * @brief   Gaussian blur implementations for image processing.
 */

#pragma once

#include "std_types.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
#include <cmath>

namespace processing
{

/**
 * @brief   Apply 5x5 Gaussian blur via full 2D convolution.
 *
 * Template parameterized on:
 *   - PixelT  : pixel type (uint8_t for grayscale)
 *   - AccumT  : accumulator type (int32_t to avoid overflow)
 *   - KernelT : kernel coefficient type (int16_t)
 *
 * Integer arithmetic only — no floating point.
 * Accumulates into AccumT, divides by 273, clamps to [0, 255].
 * Boundary handling: zero-padding (out-of-bounds pixels = 0).
 *
 * @tparam  PixelT   Pixel type.
 * @tparam  AccumT   Accumulator type.
 * @tparam  KernelT  Kernel coefficient type.
 * @param   image    The input image metadata.
 * @return  Status indicating success or failure.
 */
template <typename PixelT = uint8_t, typename AccumT = int32_t>
[[nodiscard]] Status spatial_5x5(image::io::metadata_t<PixelT>& image);

/**
 * @brief   Applies a 5x5 Gaussian blur to the input image using separable convolution.
 * @param   image The input image metadata, which will be modified in-place with the blurred output.
 * @return  Status indicating success or failure of the operation.
 */
template <typename PixelT = uint8_t, typename AccumT = int32_t>
[[nodiscard]] Status separable_5x5(image::io::metadata_t<PixelT>& image);

} // namespace processing
