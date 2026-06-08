/**
 * @file    gaussian.cpp
 * @brief   Gaussian blur implementations for image processing.
 */

#include "gaussian.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdlib>

namespace processing
{

template <typename PixelT, typename AccumT>
[[nodiscard]] Status spatial_5x5(image::io::metadata_t<PixelT>& image)
{
    // TODO: Implement 2D Gaussian blur
    return Status::E_OK;
}

template <typename PixelT, typename AccumT>
[[nodiscard]] Status separable_5x5(image::io::metadata_t<PixelT>& image)
{
    // TODO: Implement separable Gaussian blur
    return Status::E_OK;
}

// Explicit instantiations — required since implementation is in .cpp
template Status spatial_5x5<uint8_t, int32_t>(image::io::metadata_t<uint8_t>&);
template Status separable_5x5<uint8_t, int32_t>(image::io::metadata_t<uint8_t>&);

} // namespace processing
