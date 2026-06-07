/**
 * @file    gaussian.cpp
 * @brief   5x5 Gaussian blur — 2D and separable implementations.
 *
 * Both versions use integer arithmetic only.
 * Boundary handling: zero-padding throughout.
 * @author  Youssef
 */

#include "gaussian.hpp"
#include "image_utils.hpp"
#include <algorithm>
#include <cstdlib>

namespace image::gaussian
{

template <typename PixelT, typename AccumT, typename KernelT>
[[nodiscard]] Status blur_2d(const metadata_t<PixelT>& input,
                                   metadata_t<PixelT>& output)
{
    if (!input.buffer || !output.buffer)  return Status::E_INVAL_PTR;
    if (!input.pixel_count)               return Status::E_INVAL_SIZE;

    const int W = input.width;
    const int H = input.height;

    // Kernel half-size — for a 5x5 kernel this is 2
    constexpr int HALF = 2;

    for (int row = 0; row < H; ++row)
    {
        for (int col = 0; col < W; ++col)
        {
            AccumT sum = 0;

            // Accumulate over 5x5 kernel window
            for (int kr = -HALF; kr <= HALF; ++kr)
            {
                for (int kc = -HALF; kc <= HALF; ++kc)
                {
                    const int r = row + kr;
                    const int c = col + kc;

                    // Zero-padding: treat out-of-bounds as 0
                    // This is intentional — simplifies RVV vectorization in Phase 6
                    PixelT pixel = 0;
                    if (r >= 0 && r < H && c >= 0 && c < W)
                    {
                        pixel = input.buffer[r * W + c];
                    }

                    sum += static_cast<AccumT>(pixel) *
                           static_cast<AccumT>(kernels::GAUSSIAN_5X5[kr + HALF][kc + HALF]);
                }
            }

            // Normalize by 273 and clamp to [0, 255]
            sum = sum / kernels::GAUSSIAN_NORM;
            output.buffer[row * W + col] = static_cast<PixelT>(
                std::clamp(sum, static_cast<AccumT>(0), static_cast<AccumT>(255)));
        }
    }

    return Status::E_OK;
}

template <typename PixelT, typename AccumT, typename KernelT>
[[nodiscard]] Status blur_separable(const metadata_t<PixelT>& input,
                                          metadata_t<PixelT>& output)
{
    if (!input.buffer || !output.buffer)  return Status::E_INVAL_PTR;
    if (!input.pixel_count)               return Status::E_INVAL_SIZE;

    const int    W    = input.width;
    const int    H    = input.height;
    constexpr int HALF = 2;

    // Allocate intermediate buffer for horizontal pass result
    const size_t buf_size = utils::memory::align_64(input.pixel_count * sizeof(PixelT));
    void* raw = std::aligned_alloc(64, buf_size);
    if (!raw) return Status::E_ALLOC_FAIL;

    auto* temp = static_cast<PixelT*>(raw);

    // Horizontal pass — convolve each row with 1x5 kernel
    for (int row = 0; row < H; ++row)
    {
        for (int col = 0; col < W; ++col)
        {
            AccumT sum = 0;
            for (int kc = -HALF; kc <= HALF; ++kc)
            {
                const int c = col + kc;
                // Zero-padding: out-of-bounds pixels = 0
                PixelT pixel = (c >= 0 && c < W) ? input.buffer[row * W + c] : 0;
                sum += static_cast<AccumT>(pixel) *
                       static_cast<AccumT>(kernels::GAUSSIAN_1X5[kc + HALF]);
            }
            sum = sum / kernels::GAUSSIAN_1D_NORM;
            temp[row * W + col] = static_cast<PixelT>(
                std::clamp(sum, static_cast<AccumT>(0), static_cast<AccumT>(255)));
        }
    }

    // Vertical pass — convolve each column with 5x1 kernel
    for (int row = 0; row < H; ++row)
    {
        for (int col = 0; col < W; ++col)
        {
            AccumT sum = 0;
            for (int kr = -HALF; kr <= HALF; ++kr)
            {
                const int r = row + kr;
                // Zero-padding: out-of-bounds pixels = 0
                PixelT pixel = (r >= 0 && r < H) ? temp[r * W + col] : 0;
                sum += static_cast<AccumT>(pixel) *
                       static_cast<AccumT>(kernels::GAUSSIAN_1X5[kr + HALF]);
            }
            sum = sum / kernels::GAUSSIAN_1D_NORM;
            output.buffer[row * W + col] = static_cast<PixelT>(
                std::clamp(sum, static_cast<AccumT>(0), static_cast<AccumT>(255)));
        }
    }

    std::free(temp);
    return Status::E_OK;
}

// Explicit instantiations — required since implementation is in .cpp
template Status blur_2d<uint8_t, int32_t, int16_t>(
    const metadata_t<uint8_t>&, metadata_t<uint8_t>&);

template Status blur_separable<uint8_t, int32_t, int16_t>(
    const metadata_t<uint8_t>&, metadata_t<uint8_t>&);

} // namespace image::gaussian