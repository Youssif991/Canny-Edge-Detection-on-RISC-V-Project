/**
 * @file    gaussian_rvv.hpp
 * @brief   Vectorized Gaussian blur implementations for image processing using RVV.
 */

#pragma once

#include "std_types.hpp"

namespace processing
{

/**
 * @brief   Apply 5x5 Gaussian blur via full 2D convolution (RVV vectorized).
 *
 * @param   input_image  The input image metadata.
 * @return  Status indicating success or failure.
 */
Status gaussian_spatial_5x5_rvv_lmul1(image::io::metadata_t<uint8_t>& input_image);
Status gaussian_spatial_5x5_rvv_lmul2(image::io::metadata_t<uint8_t>& input_image);

// Default wrapper that uses the optimal LMUL=2 (Accumulator=8) variant
Status gaussian_spatial_5x5_rvv(image::io::metadata_t<uint8_t>& input_image);

} // namespace processing
