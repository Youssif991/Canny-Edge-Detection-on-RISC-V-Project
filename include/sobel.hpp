/**
 * @file sobel.hpp
 * @brief Interface for the sobel pipeline stage.
 * @author Youssef
 */

#pragma once

#include <cstdint>
#include "std_types.hpp"

// Compute Gx and Gy gradients from blurred image
namespace processing
{
    template <typename PixelT = uint8_t, typename OutputT = int16_t>
    Status sobel_3x3(
        const image::io::metadata_t<PixelT> &input,
        OutputT *__restrict gx,
        OutputT *__restrict gy);
}