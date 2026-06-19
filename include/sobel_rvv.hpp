#pragma once

#include "std_types.hpp"
#include <cstdint>

namespace processing
{
    /**
     * @brief Perform 3x3 Sobel edge detection using RVV intrinsics.
     *
     * Produces identical output to sobel_3x3() with replicate padding.
     *
     * @tparam LMUL  Vector length multiplier (1, 2, or 4).
     *               u8 pixels load at LMUL, gradients computed at LMUL*2
     *               due to u8->i16 widening.
     * @param image    Input grayscale image.
     * @param buffer_x Output buffer for horizontal gradient (Gx).
     * @param buffer_y Output buffer for vertical gradient (Gy).
     * @return Status indicating success or failure.
     */
    template <int LMUL = 2>
    Status sobel_3x3_rvv(const image::io::metadata_t<uint8_t> &image,
                         int16_t *__restrict buffer_x,
                         int16_t *__restrict buffer_y);
}