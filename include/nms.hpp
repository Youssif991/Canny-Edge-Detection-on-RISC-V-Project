/**
 * @file    nms.hpp
 * @brief   Non-Maximum Suppression (NMS) stage of the Canny edge detector.
 *
 * Thins edges by suppressing any pixel whose gradient magnitude is not a
 * local maximum along the gradient direction.  The direction image must
 * contain quantized angles produced by processing::Direction (0, 45, 90,
 * or 135 degrees stored as uint8_t values).
 *
 * @author  Abdel-dayem
 */

#pragma once

#include "std_types.hpp"
#include <cstdint>

namespace processing
{

/**
 * @brief  Apply non-maximum suppression to thin edges to single-pixel width.
 *
 * For each pixel the function looks at its two neighbours along the gradient
 * direction.  If the pixel is not strictly greater than both neighbours it is
 * zeroed out in the output buffer; otherwise it is copied unchanged.
 *
 * Pixels on the image border are always set to zero (no valid neighbours).
 *
 * @tparam PixelT    Pixel type of the magnitude and output buffers (uint8_t).
 * @param  mag       Normalized gradient magnitude image [0, 255].
 * @param  dir       Quantized gradient direction image (values: 0,45,90,135).
 * @param  out       Output buffer — receives the thinned edge map.
 *                   Must be pre-allocated with the same dimensions as mag.
 * @return Status::E_OK on success, or an error code on failure.
 */
template <typename PixelT = uint8_t>
Status NonMaxSuppression(const image::io::metadata_t<PixelT>& mag,
                         const image::io::metadata_t<PixelT>& dir,
                         image::io::metadata_t<PixelT>&       out);

} // namespace processing
