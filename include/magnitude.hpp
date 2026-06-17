/**
 * @file magnitude.hpp
 * @brief Interface for gradient magnitude computation.
 * @author Youssef
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "std_types.hpp"

namespace processing
{

/**
 * @brief Computes magnitude via L1 norm: |Gx| + |Gy|.
 * @note Fast and integer-only, but slightly overestimates diagonal edges.
 * * @tparam PixelT Pixel data type.
 * @tparam GradientT Input gradient data type.
 * @tparam MagntiudeT Output magnitude data type.
 * @param image Image metadata structure.
 * @param Gx Horizontal gradient buffer.
 * @param Gy Vertical gradient buffer.
 * @return Execution status.
 */
template <typename PixelT = uint8_t, typename GradientT = int16_t, typename MagntiudeT = uint16_t>
Status MagL1(const image::io::metadata_t<PixelT>& image,
             const GradientT* __restrict Gx,
             const GradientT* __restrict Gy);

/**
 * @brief Computes magnitude via L2 norm: sqrt(Gx^2 + Gy^2).
 * @note Mathematically exact; requires floating-point or fixed-point square root.
 * * @tparam PixelT Pixel data type.
 * @tparam GradientT Input gradient data type.
 * @tparam MagntiudeT Output magnitude data type.
 * @param image Image metadata structure.
 * @param Gx Horizontal gradient buffer.
 * @param Gy Vertical gradient buffer.
 * @return Execution status.
 */
template <typename PixelT = uint8_t, typename GradientT = int16_t, typename MagntiudeT = float>
Status MagL2(const image::io::metadata_t<PixelT>& image,
             const GradientT* __restrict Gx,
             const GradientT* __restrict Gy);

} // namespace processing