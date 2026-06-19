/**
 * @file    sobel_rvv.cpp
 * @brief   Sobel gradient computation — 3x3 kernel, replicate padding, RVV vectorized.
 *
 * Vectorization strategy:
 *   For each row y, load row_top / row_mid / row_bot as u8 vectors.
 *   Produce left/right shifted variants via vslide1up / vslide1down,
 *   fixing the boundary elements manually to replicate padding.
 *   Widen u8 -> i16 (vsext), then compute Gx and Gy entirely in i16.
 *   Store directly to int16_t output buffers.
 *
 * Gx = (tr - tl) + 2*(mr - ml) + (br - bl)
 * Gy = (bl + 2*bm + br) - (tl + 2*tm + tr)
 *
 * Both are computed in a single pass per row — same row data, two outputs.
 *
 * LMUL is parameterized via rvv_traits<LMUL>. The traits cover u8 (pixel
 * loads) and i16 (gradient arithmetic). Widening u8->i16 doubles the LMUL,
 * which is encoded in the trait's i16/u8 type aliases.
 */

#include "sobel_rvv.hpp"
#ifdef __riscv
#include <riscv_vector.h>
#endif
#include <algorithm>

namespace processing
{
#ifdef __riscv

// ---------------------------------------------------------------------------
// Traits
//
// u8  — pixel input type  (LMUL base)
// i16 — gradient type     (widened: LMUL * 2, because u8 -> i16 doubles width)
//
// Operations needed:
//   setvl_e8      — set vl for 8-bit elements (governs how many pixels per iter)
//   vle8_u        — load u8 pixels
//   vslide1up_u   — shift vector up by 1 (produces "left neighbour" row)
//   vslide1down_u — shift vector down by 1 (produces "right neighbour" row)
//   vfirst_u      — extract element 0 (for fixing replicate padding at left edge)
//   vlast_u       — extract element vl-1 (for fixing replicate padding at right edge)
//   vmv_s_u       — splat scalar into element 0 (used to fix slide boundary)
//   vsext_i16     — sign-extend u8 -> i16 (widen, LMUL doubles)
//   vadd_i        — add two i16 vectors
//   vsub_i        — subtract two i16 vectors
//   vsll1_i       — shift-left by 1 (multiply by 2, used for center weight)
//   vse16_i       — store i16 vector
// ---------------------------------------------------------------------------

template <int LMUL>
struct rvv_traits;

// ---- LMUL = 1 -------------------------------------------------------------
// u8=m1, i16=m2 (widened)
template <>
struct rvv_traits<1>
{
    using u8  = vuint8m1_t;
    using i8  = vint8m1_t;
    using i16 = vint16m2_t;  // widened: m1 * 2 = m2

    static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m1(n); }

    static u8  vle8_u(const uint8_t *p, size_t vl)          { return __riscv_vle8_v_u8m1(p, vl); }
    static u8  vslide1up_u(u8 v, uint8_t fill, size_t vl)   { return __riscv_vslide1up_vx_u8m1(v, fill, vl); }
    static u8  vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m1(v, fill, vl); }
    static uint8_t vmv_x_u(u8 v)                             { return __riscv_vmv_x_s_u8m1_u8(v); }

    static i16 vsext_i16(u8 v, size_t vl) { return __riscv_vsext_vf2_i16m2(__riscv_vreinterpret_v_u8m1_i8m1(v), vl); }
    static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m2(a, b, vl); }
    static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m2(a, b, vl); }
    static i16 vsll1_i(i16 v, size_t vl)       { return __riscv_vsll_vx_i16m2(v, 1, vl); }
    static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m2(p, v, vl); }
};

// ---- LMUL = 2 -------------------------------------------------------------
// u8=m2, i16=m4 (widened)
template <>
struct rvv_traits<2>
{
    using u8  = vuint8m2_t;
    using i8  = vint8m2_t;
    using i16 = vint16m4_t;  // widened: m2 * 2 = m4

    static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m2(n); }

    static u8  vle8_u(const uint8_t *p, size_t vl)          { return __riscv_vle8_v_u8m2(p, vl); }
    static u8  vslide1up_u(u8 v, uint8_t fill, size_t vl)   { return __riscv_vslide1up_vx_u8m2(v, fill, vl); }
    static u8  vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m2(v, fill, vl); }
    static uint8_t vmv_x_u(u8 v)                             { return __riscv_vmv_x_s_u8m2_u8(v); }

    static i16 vsext_i16(u8 v, size_t vl) { return __riscv_vsext_vf2_i16m4(__riscv_vreinterpret_v_u8m2_i8m2(v), vl); }
    static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m4(a, b, vl); }
    static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m4(a, b, vl); }
    static i16 vsll1_i(i16 v, size_t vl)       { return __riscv_vsll_vx_i16m4(v, 1, vl); }
    static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m4(p, v, vl); }
};

// ---- LMUL = 4 -------------------------------------------------------------
// u8=m4, i16=m8 (widened — hardware maximum)
template <>
struct rvv_traits<4>
{
    using u8  = vuint8m4_t;
    using i8  = vint8m4_t;
    using i16 = vint16m8_t;  // widened: m4 * 2 = m8

    static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m4(n); }

    static u8  vle8_u(const uint8_t *p, size_t vl)          { return __riscv_vle8_v_u8m4(p, vl); }
    static u8  vslide1up_u(u8 v, uint8_t fill, size_t vl)   { return __riscv_vslide1up_vx_u8m4(v, fill, vl); }
    static u8  vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m4(v, fill, vl); }
    static uint8_t vmv_x_u(u8 v)                             { return __riscv_vmv_x_s_u8m4_u8(v); }

    static i16 vsext_i16(u8 v, size_t vl) { return __riscv_vsext_vf2_i16m8(__riscv_vreinterpret_v_u8m4_i8m4(v), vl); }
    static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m8(a, b, vl); }
    static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m8(a, b, vl); }
    static i16 vsll1_i(i16 v, size_t vl)       { return __riscv_vsll_vx_i16m8(v, 1, vl); }
    static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m8(p, v, vl); }
};

// ---------------------------------------------------------------------------
// Helper: given a loaded row vector, produce left and right neighbour vectors
// with replicate padding applied at both ends.
//
//   left  = vslide1up(v, v[0])      — shifts right, fills element 0 with v[0]
//   right = vslide1down(v, v[vl-1]) — shifts left,  fills element vl-1 with v[vl-1]
//
// vslide1up/down fill the exposed element with a scalar we provide — so we
// extract the boundary element first and pass it as the fill value.
// ---------------------------------------------------------------------------
template <int LMUL>
static void make_neighbours(
    typename rvv_traits<LMUL>::u8 v,
    typename rvv_traits<LMUL>::u8 &left,
    typename rvv_traits<LMUL>::u8 &right,
    size_t vl)
{
    using T = rvv_traits<LMUL>;

    // left neighbour: slide vector right by 1, fill position 0 with v[0]
    uint8_t first = T::vmv_x_u(v);                        // extract v[0]
    left = T::vslide1up_u(v, first, vl);                  // [v[0], v[0], v[1], ..., v[vl-2]]

    // right neighbour: slide vector left by 1, fill last position with v[vl-1]
    // To get v[vl-1]: slidedown by (vl-1), then extract element 0
    uint8_t last = T::vmv_x_u(T::vslide1down_u(v, 0, vl - 1)); // extract v[vl-1]
    right = T::vslide1down_u(v, last, vl);                // [v[1], ..., v[vl-1], v[vl-1]]
}

// ---------------------------------------------------------------------------
// sobel_3x3_rvv<LMUL>
//
// For each row y:
//   1. Load row_top, row_mid, row_bot (u8 vectors, full row width)
//   2. Produce left/right shifted variants for each row (replicate padding)
//   3. Widen all 9 variants to i16
//   4. Gx = (tr - tl) + 2*(mr - ml) + (br - bl)
//      Gy = (bl + 2*bm + br) - (tl + 2*tm + tr)
//   5. Store Gx, Gy
//
// The full row fits in one vl iteration for typical image widths. For very
// wide images (width > VLEN*LMUL/8) the inner loop handles multiple chunks.
// ---------------------------------------------------------------------------
template <int LMUL>
Status sobel_3x3_rvv(const image::io::metadata_t<uint8_t> &input,
                     int16_t *__restrict gx,
                     int16_t *__restrict gy)
{
    using T = rvv_traits<LMUL>;

    if (!input.height || !input.width || !input.buffer || !gx || !gy)
        return input.buffer ? Status::E_NOK : Status::E_INVAL_PTR;

    const int32_t W = static_cast<int32_t>(input.width);
    const int32_t H = static_cast<int32_t>(input.height);
    const uint8_t *buf = input.buffer.get();

    for (int32_t y = 0; y < H; ++y)
    {
        const int32_t y_top = std::clamp(y - 1, 0, H - 1);
        const int32_t y_bot = std::clamp(y + 1, 0, H - 1);

        const uint8_t *row_top = buf + y_top * W;
        const uint8_t *row_mid = buf + y      * W;
        const uint8_t *row_bot = buf + y_bot  * W;

        int16_t *out_gx = gx + y * W;
        int16_t *out_gy = gy + y * W;

        int32_t x = 0;
        size_t vl;

        for (; x < W; x += static_cast<int32_t>(vl))
        {
            vl = T::setvl_e8(W - x);

            // --- Load rows ---
            typename T::u8 v_tm = T::vle8_u(row_top + x, vl);  // top-mid (center of top row)
            typename T::u8 v_mm = T::vle8_u(row_mid + x, vl);  // mid-mid
            typename T::u8 v_bm = T::vle8_u(row_bot + x, vl);  // bot-mid

            // --- Produce left/right neighbours with replicate padding ---
            typename T::u8 v_tl, v_tr;
            typename T::u8 v_ml, v_mr;
            typename T::u8 v_bl, v_br;

            make_neighbours<LMUL>(v_tm, v_tl, v_tr, vl);
            make_neighbours<LMUL>(v_mm, v_ml, v_mr, vl);
            make_neighbours<LMUL>(v_bm, v_bl, v_br, vl);

            // Fix left boundary: if this chunk starts at x==0, left neighbours
            // should replicate element 0 (already handled by vslide1up fill).
            // If x > 0, the true left neighbour is the previous chunk's last
            // element — load it directly.
            if (x > 0)
            {
                // Overwrite element 0 of each _l vector with the true left neighbour
                // using vslide1up with the actual preceding pixel as fill.
                v_tl = T::vslide1up_u(v_tm, row_top[x - 1], vl);
                v_ml = T::vslide1up_u(v_mm, row_mid[x - 1], vl);
                v_bl = T::vslide1up_u(v_bm, row_bot[x - 1], vl);
            }

            // Fix right boundary: if this chunk ends before W, the true right
            // neighbour is the next element in the row — already in memory,
            // just need to provide it as the slide fill.
            if (x + static_cast<int32_t>(vl) < W)
            {
                v_tr = T::vslide1down_u(v_tm, row_top[x + vl], vl);
                v_mr = T::vslide1down_u(v_mm, row_mid[x + vl], vl);
                v_br = T::vslide1down_u(v_bm, row_bot[x + vl], vl);
            }

            // --- Widen u8 -> i16 ---
            typename T::i16 i_tl = T::vsext_i16(v_tl, vl);
            typename T::i16 i_tr = T::vsext_i16(v_tr, vl);
            typename T::i16 i_tm = T::vsext_i16(v_tm, vl);
            typename T::i16 i_ml = T::vsext_i16(v_ml, vl);
            typename T::i16 i_mr = T::vsext_i16(v_mr, vl);
            typename T::i16 i_bl = T::vsext_i16(v_bl, vl);
            typename T::i16 i_br = T::vsext_i16(v_br, vl);
            typename T::i16 i_bm = T::vsext_i16(v_bm, vl);

            // --- Gx = (tr - tl) + 2*(mr - ml) + (br - bl) ---
            typename T::i16 gx_top = T::vsub_i(i_tr, i_tl, vl);
            typename T::i16 gx_mid = T::vsll1_i(T::vsub_i(i_mr, i_ml, vl), vl);  // *2
            typename T::i16 gx_bot = T::vsub_i(i_br, i_bl, vl);
            typename T::i16 v_gx   = T::vadd_i(T::vadd_i(gx_top, gx_mid, vl), gx_bot, vl);

            // --- Gy = (bl + 2*bm + br) - (tl + 2*tm + tr) ---
            typename T::i16 gy_bot = T::vadd_i(T::vadd_i(i_bl, T::vsll1_i(i_bm, vl), vl), i_br, vl);
            typename T::i16 gy_top = T::vadd_i(T::vadd_i(i_tl, T::vsll1_i(i_tm, vl), vl), i_tr, vl);
            typename T::i16 v_gy   = T::vsub_i(gy_bot, gy_top, vl);

            // --- Store ---
            T::vse16_i(out_gx + x, v_gx, vl);
            T::vse16_i(out_gy + x, v_gy, vl);
        }
    }

    return Status::E_OK;
}

// ---------------------------------------------------------------------------
// Explicit instantiations
// ---------------------------------------------------------------------------

template Status sobel_3x3_rvv<1>(const image::io::metadata_t<uint8_t> &,
                                  int16_t *__restrict,
                                  int16_t *__restrict);

template Status sobel_3x3_rvv<2>(const image::io::metadata_t<uint8_t> &,
                                  int16_t *__restrict,
                                  int16_t *__restrict);

template Status sobel_3x3_rvv<4>(const image::io::metadata_t<uint8_t> &,
                                  int16_t *__restrict,
                                  int16_t *__restrict);

#endif // __riscv
} // namespace processing