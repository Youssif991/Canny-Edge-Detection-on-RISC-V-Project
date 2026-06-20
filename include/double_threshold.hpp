/**
 * @file    double_threshold.hpp
 * @brief   Double-thresholding stage of the Canny edge detector.
 *
 * Classifies every pixel of the NMS output into one of three categories:
 *   - Strong edge  (pixel value == STRONG_EDGE  = 255)
 *   - Weak edge    (pixel value == WEAK_EDGE     = 128)
 *   - Suppressed   (pixel value == 0)
 *
 * Thresholds can be supplied as absolute [0,255] values or derived
 * automatically from the image histogram via Otsu's method.
 *
 * @author  Abdel-dayem
 */

#pragma once

#include "std_types.hpp"
#include <cstdint>

namespace processing
{

/// Pixel value written for strong edges after double thresholding.
static constexpr uint8_t STRONG_EDGE = 255u;

/// Pixel value written for weak edges after double thresholding.
static constexpr uint8_t WEAK_EDGE   = 128u;

/**
 * @brief  Classify pixels as strong, weak, or suppressed using two thresholds.
 *
 * Pixels with magnitude >= high_thresh  → STRONG_EDGE (255).
 * Pixels with magnitude >= low_thresh   → WEAK_EDGE   (128).
 * All other pixels                      → 0.
 *
 * The result is written back into @p nms_out in-place so no extra buffer
 * is needed.
 *
 * @tparam PixelT      Pixel type (uint8_t).
 * @param  nms_out     NMS result image; modified in-place.
 * @param  low_thresh  Lower threshold [0, 255].  Pixels below this are zeroed.
 * @param  high_thresh Upper threshold [0, 255].  Pixels at or above become 255.
 * @return Status::E_OK on success, or an error code on bad arguments.
 */
template <typename PixelT = uint8_t>
Status DoubleThreshold(image::io::metadata_t<PixelT>& nms_out,
                       PixelT                          low_thresh,
                       PixelT                          high_thresh);

/**
 * @brief  Derive low and high thresholds automatically from the image histogram
 *         (Otsu's global threshold) and then apply double thresholding.
 *
 * Otsu's method finds the single threshold that minimises intra-class variance.
 * We use   high = otsu_threshold   and   low = high * ratio   (default 0.4).
 *
 * @tparam PixelT  Pixel type (uint8_t).
 * @param  nms_out NMS result image; modified in-place.
 * @param  ratio   low/high ratio in (0, 1).  Typical value: 0.4.
 * @return Status::E_OK on success, or an error code on bad arguments.
 */
template <typename PixelT = uint8_t>
Status DoubleThresholdAuto(image::io::metadata_t<PixelT>& nms_out,
                           float                           ratio = 0.4f);

} // namespace processing
