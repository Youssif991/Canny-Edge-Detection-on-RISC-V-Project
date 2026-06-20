/**
 * @file    hysteresis.hpp
 * @brief   Hysteresis edge tracking — final stage of the Canny edge detector.
 *
 * Promotes weak-edge pixels to strong edges when they are 8-connected to at
 * least one strong-edge pixel, then suppresses all remaining weak pixels.
 * The result is a binary edge map with values 0 or STRONG_EDGE (255).
 *
 * The implementation uses an iterative flood-fill (BFS / stack-based DFS)
 * to avoid deep recursion on large images.
 *
 * @author  Abdel-dayem
 */

#pragma once

#include "std_types.hpp"
#include "double_threshold.hpp"   // for STRONG_EDGE / WEAK_EDGE constants
#include <cstdint>

namespace processing
{

/**
 * @brief  Perform hysteresis edge tracking on a double-thresholded image.
 *
 * The input @p dt_out must have been produced by DoubleThreshold() or
 * DoubleThresholdAuto() and therefore contains only the values
 * 0, WEAK_EDGE (128), or STRONG_EDGE (255).
 *
 * After the call every pixel is either 0 (no edge) or 255 (confirmed edge).
 * The operation is performed in-place.
 *
 * Algorithm:
 *   1. Seed the BFS queue with all strong-edge pixels.
 *   2. For each strong pixel, inspect all 8 neighbours.
 *   3. Any weak neighbour is promoted to STRONG_EDGE and added to the queue.
 *   4. Repeat until the queue is empty.
 *   5. Zero-out any pixel still equal to WEAK_EDGE (unreachable weak pixels).
 *
 * @tparam PixelT   Pixel type (uint8_t).
 * @param  dt_out   Double-threshold result; modified in-place to final edge map.
 * @return Status::E_OK on success, or an error code on bad arguments.
 */
template <typename PixelT = uint8_t>
Status Hysteresis(image::io::metadata_t<PixelT>& dt_out);

} // namespace processing
