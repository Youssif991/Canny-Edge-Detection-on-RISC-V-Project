/**
 * @file direction.hpp
 * @brief Interface for the direction pipeline stage.
 * @author Youssef
 */

#pragma once

#include <cstdint>
#include "std_types.hpp"

namespace processing
{

template <typename PixelT = uint8_t, typename GradientT = int16_t>
Status Direction(const image::io::metadata_t<PixelT> &image,
                 const GradientT *__restrict Gx,
                 const GradientT *__restrict Gy);

} // namespace processing