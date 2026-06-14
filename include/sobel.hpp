/**
 * @file sobel.hpp
 * @brief Interface for the sobel pipeline stage.
 * @author Youssef
 */

#pragma once

#include <cstdint>

// Compute Gx and Gy gradients from blurred image
Status sobel_3x3(
    const image::io::metadata_t<uint8_t>&  input,
          image::io::metadata_t<int16_t>&  gx,
          image::io::metadata_t<int16_t>&  gy);