/**
 * @file magnitude.cpp
 * @brief Gradient magnitude — L1 and L2 norm implementations.
 * @author Youssef
 */

#include "magnitude.hpp"
#include <memory>

/**
 * @brief Computes L1 magnitude (|Gx| + |Gy|) and normalizes output to [0, 255].
 * @tparam PixelT Output pixel data type.
 * @tparam GradientT Input gradient data type.
 * @tparam MagntiudeT Intermediate magnitude data type.
 * @param[in,out] image Image metadata containing dimensions and destination buffer.
 * @param[in] Gx Horizontal gradient buffer.
 * @param[in] Gy Vertical gradient buffer.
 * @return Execution status code.
 */
template <typename PixelT, typename GradientT, typename MagntiudeT>
Status MagL1(const image::io::metadata_t<PixelT> &image,
             const GradientT *__restrict Gx,
             const GradientT *__restrict Gy)
{
    if (!image.height || !image.width || !image.buffer)
    {
        return image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
    }
    std::unique_ptr<MagntiudeT[], utils::memory::deleter> magnitude_buffer(utils::memory::aligned_alloc(64,
                                                                                                        utils::memory::align_64(image.pixel_count * sizeof(MagntiudeT))));
    if (!magnitude_buffer)
    {
        return Status::E_ALLOC_FAIL;
    }
    MagntiudeT max_magnitude = 0;
    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        MagntiudeT raw_mag = static_cast<MagntiudeT>(std::abs(Gx[i]) + std::abs(Gy[i]));
        magnitude_buffer[i] = raw_mag;

        if (raw_mag > max_magnitude)
        {
            max_magnitude = raw_mag;
        }
    }

    PixelT *__restrict out_ptr = image.buffer.get();

    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        magitude_buffer[i] = static_cast<PixelT>((magnitude_buffer[i] * 255) / max_magnitude);
    }

    return Status::E_OK;
}
