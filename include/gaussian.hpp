/**
 * @file    gaussian.hpp
 * @brief   5x5 Gaussian blur — 2D convolution and separable filter versions.
 *
 * Boundary handling: zero-padding throughout.
 * Out-of-bounds pixels are treated as 0 in all convolution operations.
 * This simplifies vectorization in Phase 6.
 *
 * @author  Youssef
 */

#pragma once
#include "image_types.hpp"

namespace image::gaussian
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
 * @param   input    Source image metadata (width * height pixels).
 * @param   output   Destination image metadata (same dimensions).
 * @return  Status::E_OK on success, error code otherwise.
 */
template <typename PixelT  = uint8_t,
          typename AccumT  = int32_t,
          typename KernelT = int16_t>
[[nodiscard]] Status blur_2d(const metadata_t<PixelT>& input,
                                   metadata_t<PixelT>& output);

/**
 * @brief   Apply 5x5 Gaussian blur via separable 1D passes.
 *
 * Decomposes the 2D kernel into a horizontal 1x5 pass followed by
 * a vertical 5x1 pass. Reduces multiply-accumulate operations from
 * 25 to 10 per pixel. Requires an intermediate buffer.
 *
 * Boundary handling: zero-padding (out-of-bounds pixels = 0).
 *
 * @tparam  PixelT   Pixel type.
 * @tparam  AccumT   Accumulator type.
 * @tparam  KernelT  Kernel coefficient type.
 * @param   input    Source image metadata.
 * @param   output   Destination image metadata.
 * @return  Status::E_OK on success, error code otherwise.
 */
template <typename PixelT  = uint8_t,
          typename AccumT  = int32_t,
          typename KernelT = int16_t>
[[nodiscard]] Status blur_separable(const metadata_t<PixelT>& input,
                                          metadata_t<PixelT>& output);

} // namespace image::gaussian