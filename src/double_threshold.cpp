/**
 * @file    double_threshold.cpp
 * @brief   Double thresholding — scalar implementation.
 *
 * Classifies NMS output pixels into strong (255), weak (128), or suppressed
 * (0) based on two user-supplied or automatically derived thresholds.
 *
 * DoubleThresholdAuto() uses Otsu's global threshold as the high threshold
 * and derives the low threshold as   low = high * ratio   (default 0.4).
 *
 * @author  Abdel-dayem
 */

#include "double_threshold.hpp"
#include <cstdint>
#include <algorithm>
#include <array>

namespace processing
{

// ─────────────────────────────────────────────────────────────────────────────
// DoubleThreshold — explicit thresholds
// ─────────────────────────────────────────────────────────────────────────────

template <typename PixelT>
Status DoubleThreshold(image::io::metadata_t<PixelT>& nms_out,
                       PixelT                          low_thresh,
                       PixelT                          high_thresh)
{
    if (!nms_out.buffer)
        return Status::E_INVAL_PTR;

    if (nms_out.width == 0 || nms_out.height == 0)
        return Status::E_INVAL_SIZE;

    if (low_thresh >= high_thresh)
        return Status::E_NOK;

    PixelT* __restrict buf = nms_out.buffer.get();
    const uint32_t      n  = nms_out.pixel_count;

    for (uint32_t i = 0; i < n; ++i)
    {
        const PixelT v = buf[i];

        if (v >= high_thresh)
            buf[i] = STRONG_EDGE;
        else if (v >= low_thresh)
            buf[i] = WEAK_EDGE;
        else
            buf[i] = PixelT{0};
    }

    return Status::E_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// DoubleThresholdAuto — Otsu-derived thresholds
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief  Compute Otsu's global threshold from a uint8_t histogram.
 *
 * Minimises intra-class variance = maximises inter-class variance.
 *
 * @param  hist   256-bin histogram of pixel values.
 * @param  total  Total number of pixels.
 * @return Otsu threshold in [0, 255].
 */
static uint8_t otsu_threshold(const uint32_t hist[256], uint32_t total)
{
    // Weighted sum of all levels
    uint64_t sum_all = 0;
    for (int i = 0; i < 256; ++i)
        sum_all += static_cast<uint64_t>(i) * hist[i];

    uint64_t sum_bg  = 0;
    uint32_t cnt_bg  = 0;
    double   max_var = 0.0;
    uint8_t  thresh  = 0;

    for (int t = 0; t < 256; ++t)
    {
        cnt_bg  += hist[t];
        if (cnt_bg == 0) continue;

        const uint32_t cnt_fg = total - cnt_bg;
        if (cnt_fg == 0) break;

        sum_bg += static_cast<uint64_t>(t) * hist[t];

        const double mean_bg = static_cast<double>(sum_bg)  / cnt_bg;
        const double mean_fg = static_cast<double>(sum_all - sum_bg) / cnt_fg;
        const double delta   = mean_fg - mean_bg;

        const double var = static_cast<double>(cnt_bg) *
                           static_cast<double>(cnt_fg) *
                           delta * delta;

        if (var > max_var)
        {
            max_var = var;
            thresh  = static_cast<uint8_t>(t);
        }
    }

    return thresh;
}

template <typename PixelT>
Status DoubleThresholdAuto(image::io::metadata_t<PixelT>& nms_out,
                           float                           ratio)
{
    if (!nms_out.buffer)
        return Status::E_INVAL_PTR;

    if (nms_out.width == 0 || nms_out.height == 0)
        return Status::E_INVAL_SIZE;

    if (ratio <= 0.0f || ratio >= 1.0f)
        return Status::E_NOK;

    const PixelT*  buf   = nms_out.buffer.get();
    const uint32_t n     = nms_out.pixel_count;

    // Build histogram
    uint32_t hist[256] = {};
    for (uint32_t i = 0; i < n; ++i)
        ++hist[static_cast<uint8_t>(buf[i])];

    const uint8_t high = otsu_threshold(hist, n);
    const uint8_t low  = static_cast<uint8_t>(static_cast<float>(high) * ratio);

    // Guard against degenerate thresholds (e.g. blank image)
    if (high == 0 || low >= high)
        return DoubleThreshold<PixelT>(nms_out, PixelT{20}, PixelT{50});

    return DoubleThreshold<PixelT>(nms_out, static_cast<PixelT>(low),
                                             static_cast<PixelT>(high));
}

// ── Explicit instantiations ───────────────────────────────────────────────────
template Status DoubleThreshold<uint8_t>(
    image::io::metadata_t<uint8_t>&, uint8_t, uint8_t);

template Status DoubleThresholdAuto<uint8_t>(
    image::io::metadata_t<uint8_t>&, float);

} // namespace processing
