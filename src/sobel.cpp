/**
 * @file    sobel.cpp
 * @brief   Sobel gradient computation — 3x3 kernel, replicate padding.
 * @author  Youssef
 */

#include "sobel.hpp"
#include <algorithm>

namespace processing
{

    template <typename PixelT, typename OutputT>
    Status sobel_3x3(
        const image::io::metadata_t<PixelT> &input,
        image::io::metadata_t<OutputT> &gx,
        image::io::metadata_t<OutputT> &gy)
    {
        if (!input.height || !input.width || !input.buffer)
        {
            return input.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        const int32_t W = static_cast<int32_t>(input.width);
        const int32_t H = static_cast<int32_t>(input.height);

        for (int32_t y = 0; y < H; ++y)
        {
            // Clamp row indices to valid range — replicate padding at borders
            const int32_t y_top = std::clamp(y - 1, 0, H - 1);
            const int32_t y_bot = std::clamp(y + 1, 0, H - 1);

            // Pre-compute row pointers once per row
            const PixelT *row_top = &input.buffer.get()[y_top * W];
            const PixelT *row_mid = &input.buffer.get()[y * W];
            const PixelT *row_bot = &input.buffer.get()[y_bot * W];

            OutputT *out_gx = &gx.buffer.get()[y * W];
            OutputT *out_gy = &gy.buffer.get()[y * W];

            for (int32_t x = 0; x < W; ++x)
            {
                // Clamp column indices to valid range — replicate padding at borders
                const int32_t x_l = std::clamp(x - 1, 0, W - 1);
                const int32_t x_r = std::clamp(x + 1, 0, W - 1);

                // Gx — horizontal gradient (detects vertical edges)
                // Kernel: [-1 0 +1 / -2 0 +2 / -1 0 +1]
                out_gx[x] = static_cast<OutputT>(
                    (row_top[x_r] - row_top[x_l]) +
                    ((row_mid[x_r] - row_mid[x_l]) << 1) +
                    (row_bot[x_r] - row_bot[x_l]));

                // Gy — vertical gradient (detects horizontal edges)
                // Kernel: [-1 -2 -1 / 0 0 0 / +1 +2 +1]
                out_gy[x] = static_cast<OutputT>(
                    (row_top[x_l] + (row_top[x] << 1) + row_top[x_r]) -
                    (row_bot[x_l] + (row_bot[x] << 1) + row_bot[x_r]));
            }
        }

        return Status::E_OK;
    }
} // namespace processing