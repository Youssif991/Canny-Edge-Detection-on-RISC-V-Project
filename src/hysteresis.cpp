/**
 * @file    hysteresis.cpp
 * @brief   Hysteresis edge tracking — scalar iterative BFS implementation.
 *
 * Starting from every strong-edge seed, the algorithm propagates confirmations
 * to 8-connected weak neighbours using a stack (DFS) to avoid heap allocations
 * from std::queue and to keep memory access more cache-friendly.
 *
 * @author  Abdel-dayem
 */

#include "hysteresis.hpp"
#include <cstdint>
#include <vector>
#include <cstring>

namespace processing
{

template <typename PixelT>
Status Hysteresis(image::io::metadata_t<PixelT>& dt_out)
{
    // ── Validate ──────────────────────────────────────────────────────────
    if (!dt_out.buffer)
        return Status::E_INVAL_PTR;

    if (dt_out.width == 0 || dt_out.height == 0)
        return Status::E_INVAL_SIZE;

    PixelT* __restrict buf = dt_out.buffer.get();
    const uint32_t W       = dt_out.width;
    const uint32_t H       = dt_out.height;
    const uint32_t n       = dt_out.pixel_count;

    // ── Seed stack with all strong-edge pixels ────────────────────────────
    // Reserve a reasonable fraction upfront; the vector will grow if needed.
    std::vector<uint32_t> stack;
    stack.reserve(n / 8);

    for (uint32_t i = 0; i < n; ++i)
    {
        if (static_cast<uint8_t>(buf[i]) == STRONG_EDGE)
            stack.push_back(i);
    }

    // ── 8-neighbour offsets ───────────────────────────────────────────────
    // Listed as signed (row_delta, col_delta) pairs:
    //   NW  N  NE
    //    W  *  E
    //   SW  S  SE
    static constexpr int8_t DR[8] = {-1, -1, -1,  0,  0,  1,  1,  1};
    static constexpr int8_t DC[8] = {-1,  0,  1, -1,  1, -1,  0,  1};

    // ── BFS/DFS flood-fill ────────────────────────────────────────────────
    while (!stack.empty())
    {
        const uint32_t idx = stack.back();
        stack.pop_back();

        const uint32_t r = idx / W;
        const uint32_t c = idx % W;

        for (int k = 0; k < 8; ++k)
        {
            const int32_t nr = static_cast<int32_t>(r) + DR[k];
            const int32_t nc = static_cast<int32_t>(c) + DC[k];

            // Bounds check
            if (nr < 0 || nc < 0 ||
                static_cast<uint32_t>(nr) >= H ||
                static_cast<uint32_t>(nc) >= W)
                continue;

            const uint32_t nidx = static_cast<uint32_t>(nr) * W +
                                   static_cast<uint32_t>(nc);

            if (static_cast<uint8_t>(buf[nidx]) == WEAK_EDGE)
            {
                buf[nidx] = static_cast<PixelT>(STRONG_EDGE);
                stack.push_back(nidx);
            }
        }
    }

    // ── Suppress remaining weak pixels ────────────────────────────────────
    for (uint32_t i = 0; i < n; ++i)
    {
        if (static_cast<uint8_t>(buf[i]) == WEAK_EDGE)
            buf[i] = PixelT{0};
    }

    return Status::E_OK;
}

// ── Explicit instantiation ────────────────────────────────────────────────────
template Status Hysteresis<uint8_t>(image::io::metadata_t<uint8_t>&);

} // namespace processing
