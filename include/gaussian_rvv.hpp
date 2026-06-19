/**
 * @file    gaussian_rvv.hpp
 * @brief   Vectorized Gaussian blur — RVV, fixed LMUL=2.
 */

#pragma once

#include "std_types.hpp"

namespace processing
{

/**
 * @brief Apply 5x5 Gaussian blur via full 2D convolution (RVV vectorized, LMUL=2).
 * @param input_image  The input image metadata (modified in-place).
 * @return Status indicating success or failure.
 */
Status gaussian_spatial_5x5_rvv(image::io::metadata_t<uint8_t>& input_image);

} // namespace processing