/**
 * @file    nms.cpp
 * @brief   Non-Maximum Suppression — scalar implementation.
 *
 * For each interior pixel the gradient magnitude is compared with its two
 * direct neighbours along the quantized gradient direction (0°, 45°, 90°,
 * 135°).  A pixel survives only if it is strictly greater than both
 * neighbours; otherwise its output is set to zero.
 *
 * Neighbour offsets for each direction (row_delta, col_delta):
 *   0°   → (0, +1) and (0, -1)   — horizontal edge, check left/right
 *   45°  → (-1,+1) and (+1,-1)   — NE/SW diagonal
 *   90°  → (-1, 0) and (+1, 0)   — vertical edge, check up/down
 *   135° → (-1,-1) and (+1,+1)   — NW/SE diagonal
 *
 * @author  Abdel-dayem
 */

#include "nms.hpp"
#include <cstdint>
#include <algorithm>

namespace processing
{

template <typename PixelT>
Status NonMaxSuppression(const image::io::metadata_t<PixelT>& mag,
                         const image::io::metadata_t<PixelT>& dir,
                         image::io::metadata_t<PixelT>&       out)
{
    // ── Validate inputs ───────────────────────────────────────────────────
    if (!mag.buffer || !dir.buffer || !out.buffer)
        return Status::E_INVAL_PTR;

    if (mag.width == 0 || mag.height == 0)
        return Status::E_INVAL_SIZE;

    if (dir.width  != mag.width  || dir.height  != mag.height  ||
        out.width  != mag.width  || out.height  != mag.height)
        return Status::E_NOK;

    const uint32_t W = mag.width;
    const uint32_t H = mag.height;

    const PixelT* __restrict mag_buf = mag.buffer.get();
    const PixelT* __restrict dir_buf = dir.buffer.get();
    PixelT*       __restrict out_buf = out.buffer.get();

    // ── Border pixels → always zero ───────────────────────────────────────
    // Top and bottom rows
    for (uint32_t c = 0; c < W; ++c)
    {
        out_buf[c]                   = PixelT{0};   // row 0
        out_buf[(H - 1) * W + c]    = PixelT{0};   // row H-1
    }
    // Left and right columns (excluding corners already set above)
    for (uint32_t r = 1; r < H - 1; ++r)
    {
        out_buf[r * W]           = PixelT{0};   // col 0
        out_buf[r * W + W - 1]  = PixelT{0};   // col W-1
    }

    // ── Interior pixels ───────────────────────────────────────────────────
    for (uint32_t r = 1; r < H - 1; ++r)
    {
        for (uint32_t c = 1; c < W - 1; ++c)
        {
            const uint32_t idx = r * W + c;
            const PixelT   mag_val = mag_buf[idx];
            const uint8_t  angle   = static_cast<uint8_t>(dir_buf[idx]);

            PixelT n1{0}, n2{0};

            switch (angle)
            {
                case 0:   // horizontal — compare left / right
                    n1 = mag_buf[idx - 1];
                    n2 = mag_buf[idx + 1];
                    break;

                case 45:  // NE/SW diagonal
                    n1 = mag_buf[(r - 1) * W + (c + 1)];
                    n2 = mag_buf[(r + 1) * W + (c - 1)];
                    break;

                case 90:  // vertical — compare up / down
                    n1 = mag_buf[(r - 1) * W + c];
                    n2 = mag_buf[(r + 1) * W + c];
                    break;

                case 135: // NW/SE diagonal
                    n1 = mag_buf[(r - 1) * W + (c - 1)];
                    n2 = mag_buf[(r + 1) * W + (c + 1)];
                    break;

                default:  // unexpected angle — suppress
                    n1 = n2 = PixelT{255};
                    break;
            }

            // Suppress if not a local maximum
            out_buf[idx] = (mag_val >= n1 && mag_val >= n2) ? mag_val : PixelT{0};
        }
    }

    return Status::E_OK;
}

// ── Explicit instantiation ────────────────────────────────────────────────────
template Status NonMaxSuppression<uint8_t>(
    const image::io::metadata_t<uint8_t>&,
    const image::io::metadata_t<uint8_t>&,
    image::io::metadata_t<uint8_t>&);

} // namespace processing
