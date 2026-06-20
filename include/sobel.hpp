/**
 * @file    sobel.hpp
 * @brief   Sobel edge detection implementations for image processing.
 */

#pragma once

#include "std_types.hpp"
#include <cstdint>

namespace processing
{
/** @brief Perform 3x3 Sobel edge detection on the input image.
 * @param image The input image metadata.
 * @param buffer_x The buffer to store the x-direction gradients.
 * @param buffer_y The buffer to store the y-direction gradients.
 * @return Status indicating success or failure.
 */
template <typename PixelT = uint8_t, typename OutputT = int16_t>
 Status sobel_3x3(const image::io::metadata_t<PixelT>& image,
                                OutputT* __restrict buffer_x,
                                OutputT* __restrict buffer_y);

} // namespace processing