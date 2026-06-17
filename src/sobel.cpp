/**
 * @file    sobel.cpp
 * @brief   Sobel gradient computation — 3x3 kernel, replicate padding.
 * @author  Youssef
 */

#include "sobel.hpp"
#include <algorithm>

namespace processing
{

    /**
     * @brief   Compute Sobel gradients Gx and Gy from a grayscale image.
     *
     * Applies the 3x3 Sobel operator to produce signed gradient buffers.
     * Both gradients are computed in a single pass over the image.
     *
     * Boundary handling: replicate padding — edge pixels are repeated rather
     * than treated as zero, which avoids fake edges at image borders.
     *
     * Kernel — Gx (detects vertical edges, measures horizontal change):
     * @code
     * -1   0  +1
     * -2   0  +2
     * -1   0  +1
     * @endcode
     *
     * Kernel — Gy (detects horizontal edges, measures vertical change):
     * @code
     * -1  -2  -1
     * 0   0   0
     * +1  +2  +1
     * @endcode
     *
     * @tparam  PixelT  Input pixel type (default: uint8_t for grayscale).
     * @tparam  OutputT Gradient output type (default: int16_t — signed,
     * sufficient for max Sobel value of ±2040).
     * @param   input   Blurred grayscale input image.
     * @param   gx      Output buffer for horizontal gradient (Gx).
     * @param   gy      Output buffer for vertical gradient (Gy).
     * @return  Status::E_OK on success, error code otherwise.
     */
    template <typename PixelT, typename OutputT>
    Status sobel_3x3(
        const image::io::metadata_t<PixelT> &input,
        OutputT* __restrict gx,
        OutputT* __restrict gy)
    {
        
        if (!input.height || !input.width || !input.buffer || !gx || !gy)
        {
            return input.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        const int32_t W = static_cast<int32_t>(input.width);
        const int32_t H = static_cast<int32_t>(input.height);

        for (int32_t y = 0; y < H; ++y)
        {
            const int32_t y_top = std::clamp(y - 1, 0, H - 1);
            const int32_t y_bot = std::clamp(y + 1, 0, H - 1);

            const PixelT *row_top = &input.buffer.get()[y_top * W];
            const PixelT *row_mid = &input.buffer.get()[y * W];
            const PixelT *row_bot = &input.buffer.get()[y_bot * W];

            
            OutputT *out_gx = &gx[y * W];
            OutputT *out_gy = &gy[y * W];

            for (int32_t x = 0; x < W; ++x)
            {
                const int32_t x_l = std::clamp(x - 1, 0, W - 1);
                const int32_t x_r = std::clamp(x + 1, 0, W - 1);

                
                out_gx[x] = static_cast<OutputT>(
                    (row_top[x_r] - row_top[x_l]) +
                    ((row_mid[x_r] - row_mid[x_l]) << 1) +
                    (row_bot[x_r] - row_bot[x_l]));

                
                out_gy[x] = static_cast<OutputT>(
                    (row_bot[x_l] + (row_bot[x] << 1) + row_bot[x_r]) -
                    (row_top[x_l] + (row_top[x] << 1) + row_top[x_r]));
            }
        }

        return Status::E_OK;
    }

     /**
     * @brief   Compute Sobel gradients Gx and Gy from a pre-padded grayscale image.
     *
     * Similar to sobel_3x3, but expects the input image to already be padded
     * (e.g. 1 pixel replicate padding on all sides). The input metadata reflects
     * the unpadded dimensions, but the buffer pointer and stride correspond
     * to the padded buffer. Wait, actually, let's just make it do internal padding
     * to make it a fair drop-in replacement, or assume the padded buffer is passed
     * in and the loop bounds are adjusted.
     * As per the guide: "Try removing the boundary check from your inner loop by pre-padding
     * the image with zeros... pad with replicated border pixels, then remove the std::clamp calls".
     *
     * For this function, we will allocate a padded buffer internally, pad it with replicate
     * border pixels, and then run the clamp-free loop to see if the compiler auto-vectorizes.
     */
    template <typename PixelT, typename OutputT>
    Status sobel_3x3_unbounded(
        const image::io::metadata_t<PixelT> &input,
        OutputT* __restrict gx,
        OutputT* __restrict gy)
    {
        if (!input.height || !input.width || !input.buffer || !gx || !gy)
        {
            return input.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        const int32_t W = static_cast<int32_t>(input.width);
        const int32_t H = static_cast<int32_t>(input.height);

        const int32_t padded_w = W + 2;
        const int32_t padded_h = H + 2;
        const uint32_t padded_size = padded_w * padded_h;

        // Allocate 64-byte aligned padded buffer
        auto padded_ptr = static_cast<PixelT*>(
            utils::memory::aligned_alloc(64, utils::memory::align_64(padded_size * sizeof(PixelT))));
        if (!padded_ptr) return Status::E_ALLOC_FAIL;
        
        std::unique_ptr<PixelT[], utils::memory::deleter> padded_img(padded_ptr);

        // Fill padded buffer with replicate padding
        for (int32_t y = -1; y <= H; ++y)
        {
            int32_t clamped_y = std::clamp(y, 0, H - 1);
            for (int32_t x = -1; x <= W; ++x)
            {
                int32_t clamped_x = std::clamp(x, 0, W - 1);
                padded_ptr[(y + 1) * padded_w + (x + 1)] = input.buffer.get()[clamped_y * W + clamped_x];
            }
        }

        // Now run the inner loop without any std::clamp or boundary checks
        for (int32_t y = 0; y < H; ++y)
        {
            const PixelT *row_top = &padded_ptr[y * padded_w];
            const PixelT *row_mid = &padded_ptr[(y + 1) * padded_w];
            const PixelT *row_bot = &padded_ptr[(y + 2) * padded_w];

            OutputT *out_gx = &gx[y * W];
            OutputT *out_gy = &gy[y * W];

            for (int32_t x = 0; x < W; ++x)
            {
                const int32_t px   = x + 1; // Center pixel in padded row
                const int32_t px_l = px - 1;
                const int32_t px_r = px + 1;

                out_gx[x] = static_cast<OutputT>(
                    (row_top[px_r] - row_top[px_l]) +
                    ((row_mid[px_r] - row_mid[px_l]) << 1) +
                    (row_bot[px_r] - row_bot[px_l]));

                out_gy[x] = static_cast<OutputT>(
                    (row_bot[px_l] + (row_bot[px] << 1) + row_bot[px_r]) -
                    (row_top[px_l] + (row_top[px] << 1) + row_top[px_r]));
            }
        }

        return Status::E_OK;
    }

    
    template Status sobel_3x3<uint8_t, int16_t>(
        const image::io::metadata_t<uint8_t> &,
        int16_t *,
        int16_t *);

    template Status sobel_3x3_unbounded<uint8_t, int16_t>(
        const image::io::metadata_t<uint8_t> &,
        int16_t *,
        int16_t *);

} // namespace processing