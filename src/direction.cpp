/**
 * @file direction.cpp
 * @brief Gradient direction quantized to 0, 45, 90, 135 degrees.
 * @author Youssef
 */

#include "direction.hpp"

template <typename PixelT = uint8_t, typename GradientT = int16_t>
Status Direction(const image::io::metadata_t<PixelT> &image,
                 const GradientT *__restrict Gx,
                 const GradientT *__restrict Gy)
{
    if (!input_image.height || !input_image.width || !input_image.buffer)
    {
        return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
    }

    uint8_t angle = 0;
    const uint32_t pixel_count = image.pixel_count;
    for (uint32_t i = 0; i < pixel_count; i++)
    {
        const GradientT cur_x = gx_ptr[i];
        const GradientT cur_y = gy_ptr[i];
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
    return status::E_OK;
}
