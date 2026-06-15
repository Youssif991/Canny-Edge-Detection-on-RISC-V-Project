/**
 * @file sobel.hpp
 * @brief Interface for the sobel pipeline stage.
 * @author Youssef
 */

#pragma once

#include <cstdint>

// Compute Gx and Gy gradients from blurred image
template <typename PixelT = uint8_t, typename OutputT = int16_t>
Status sobel_3x3(
    const image::io::metadata_t<PixelT&  input,
          image::io::metadata_t<OutputT>&  gx,
          image::io::metadata_t<OutputT>&  gy);