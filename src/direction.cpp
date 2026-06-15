/**
 * @file direction.cpp
 * @brief Gradient direction quantized to 0, 45, 90, 135 degrees.
 * @author Youssef
 */

#include "direction.hpp"

/**
 * @brief Quantize gradient direction to 0°, 45°, 90°, or 135°.
 *
 * Uses ratio |Gy|/|Gx| with thresholds 2.4 (vertical) and 0.5 (diagonal).
 * Diagonal angle depends on sign of Gx and Gy.
 *
 * @param image Output buffer (stores angle per pixel)
 * @param Gx    Horizontal gradient buffer
 * @param Gy    Vertical gradient buffer
 * @return Status::E_OK on success, error code otherwise
 */

namespace processing
{

    template <typename PixelT, typename GradientT>
    Status Direction(const image::io::metadata_t<PixelT> &image,
                     const GradientT *__restrict Gx,
                     const GradientT *__restrict Gy)
    {
        if (!image.height || !image.width || !image.buffer)
        {
            return image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        uint8_t angle = 0;
        const uint32_t pixel_count = image.pixel_count;
        for (uint32_t i = 0; i < pixel_count; i++)
        {
            const GradientT cur_x = Gx[i];
            const GradientT cur_y = Gy[i];
            const uint16_t abs_x = static_cast<uint16_t>(std::abs(cur_x));
            const uint16_t abs_y = static_cast<uint16_t>(std::abs(cur_y));

            const bool is_vertical = (abs_y * 5) > (abs_x * 12);
            const bool is_diagonal = (abs_y * 2) > abs_x;
            const bool same_sign = ((cur_x > 0) && (cur_y > 0)) || ((cur_x < 0) && (cur_y < 0));

            const uint8_t diagonal_angle = same_sign ? 135 : 45;
            const uint8_t fallback_angle = is_diagonal ? diagonal_angle : 0;

            angle = is_vertical ? 90 : fallback_angle;

            image.buffer[i] = angle;
        }
        return Status::E_OK;
    }
    template <uint8_t, int16_t>
    Status Direction(const image::io::metadata_t<uint8_t> &,
                     const int16_t *__restrict,
                     const int16_t *__restrict);
}