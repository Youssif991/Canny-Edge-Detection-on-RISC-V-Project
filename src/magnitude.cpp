/**
 * @file magnitude.cpp
 * @brief Gradient magnitude — L1 and L2 norm implementations.
 * @author Youssef
 */

#include "magnitude.hpp"
#include <memory>

namespace processing
{
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

        auto mag_raw = static_cast<MagntiudeT *>(
            utils::memory::aligned_alloc(64,
                                         utils::memory::align_64(image.pixel_count * sizeof(MagntiudeT))));
        if (!mag_raw)
            return Status::E_ALLOC_FAIL;

        std::unique_ptr<MagntiudeT[], utils::memory::deleter> magnitude_buffer(mag_raw);
        if (!magnitude_buffer)
        {
            return Status::E_ALLOC_FAIL;
        }

        MagntiudeT max_magnitude = 0;
        const uint32_t pixel_count = image.pixel_count;

        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            MagntiudeT raw_mag = static_cast<MagntiudeT>(std::abs(Gx[i]) + std::abs(Gy[i]));
            magnitude_buffer[i] = raw_mag;

            if (raw_mag > max_magnitude)
            {
                max_magnitude = raw_mag;
            }
        }

        if (max_magnitude == 0)
        {
            std::fill(image.buffer.get(), image.buffer.get() + pixel_count, PixelT{0});
            return Status::E_OK;
        }

        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            image.buffer.get()[i] = static_cast<PixelT>((magnitude_buffer[i] * 255) / max_magnitude);
        }

        return Status::E_OK;
    }

    /**
     * @brief Compute L2 gradient magnitude (Euclidean norm) and normalize to [0,255].
     *
     * Magnitude = sqrt(Gx² + Gy²). Normalizes by dividing by max magnitude.
     *
     * @param image Output buffer (stores normalized magnitude per pixel)
     * @param Gx    Horizontal gradient buffer
     * @param Gy    Vertical gradient buffer
     * @return Status::E_OK on success, error code otherwise
     */
    template <typename PixelT, typename GradientT, typename MagntiudeT>
    Status MagL2(const image::io::metadata_t<PixelT> &image,
                 const GradientT *__restrict Gx,
                 const GradientT *__restrict Gy)
    {
        if (!image.height || !image.width || !image.buffer)
        {
            return image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        auto mag_raw = static_cast<MagntiudeT *>(
            utils::memory::aligned_alloc(64,
                                         utils::memory::align_64(image.pixel_count * sizeof(MagntiudeT))));
        if (!mag_raw)
            return Status::E_ALLOC_FAIL;

        std::unique_ptr<MagntiudeT[], utils::memory::deleter> magnitude_buffer(mag_raw);
        if (!magnitude_buffer)
        {
            return Status::E_ALLOC_FAIL;
        }

        MagntiudeT max_magnitude = 0.0f;
        const uint32_t pixel_count = image.pixel_count;

        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            MagntiudeT raw_mag = std::sqrt(static_cast<MagntiudeT>(Gx[i] * Gx[i] + Gy[i] * Gy[i]));
            magnitude_buffer[i] = raw_mag;

            if (raw_mag > max_magnitude)
            {
                max_magnitude = raw_mag;
            }
        }

        if (max_magnitude == 0.0f)
        {
            std::fill(image.buffer.get(), image.buffer.get() + pixel_count, PixelT{0});
            return Status::E_OK;
        }

        for (uint32_t i = 0; i < pixel_count; ++i)
        {
            image.buffer.get()[i] = static_cast<PixelT>((magnitude_buffer[i] * 255) / max_magnitude);
        }

        return Status::E_OK;
    }

    template Status MagL1<uint8_t, int16_t, uint16_t>(const image::io::metadata_t<uint8_t> &,
                                                      const int16_t *__restrict,
                                                      const int16_t *__restrict);

    template Status MagL2<uint8_t, int16_t, float>(const image::io::metadata_t<uint8_t> &,
                                                   const int16_t *__restrict,
                                                   const int16_t *__restrict);
}
