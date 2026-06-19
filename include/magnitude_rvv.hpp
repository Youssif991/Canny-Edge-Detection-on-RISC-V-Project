#pragma once

#include "std_types.hpp"

namespace processing
{
    /**
     * @brief Computes L1 magnitude (|Gx| + |Gy|) and normalizes output to [0, 255] using RVV.
     *
     * @tparam LMUL  Vector length multiplier (1, 2, or 4). Higher values process more
     *               elements per iteration at the cost of fewer available vector registers.
     * @param image  Output image metadata containing dimensions and destination buffer.
     * @param Gx     Horizontal gradient buffer.
     * @param Gy     Vertical gradient buffer.
     * @return Execution status code.
     */
    template <int LMUL = 4>
    Status MagL1(const image::io::metadata_t<uint8_t> &image,
                 const int16_t *__restrict Gx,
                 const int16_t *__restrict Gy);
}