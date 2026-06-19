/**
 * @file    sobel_rvv.cpp
 * @brief   Sobel gradient computation — 3x3 kernel, replicate padding, RVV vectorized.
 *
 * Vectorization strategy:
 * For each row y, load row_top / row_mid / row_bot as u8 vectors.
 * Produce left/right shifted variants via vslide1up / vslide1down,
 * fixing the boundary elements manually to replicate padding.
 * Widen u8 -> i16 (vzext), then compute Gx and Gy entirely in i16.
 * Store directly to int16_t output buffers.
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

    template <int LMUL>
    struct rvv_traits;

    // ---- LMUL = 1 -------------------------------------------------------------
    // u8=m1, i16=m2 (widened)
    template <>
    struct rvv_traits<1>
    {
        using u8 = vuint8m1_t;
        using i8 = vint8m1_t;
        using i16 = vint16m2_t; // widened: m1 * 2 = m2

        static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m1(n); }

        static u8 vle8_u(const uint8_t *p, size_t vl) { return __riscv_vle8_v_u8m1(p, vl); }
        static u8 vslide1up_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1up_vx_u8m1(v, fill, vl); }
        static u8 vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m1(v, fill, vl); }
        static uint8_t vmv_x_u(u8 v) { return __riscv_vmv_x_s_u8m1_u8(v); }

        // Zero-extend u8 to u16, then reinterpret as signed i16 for gradient math
        static i16 vzext_i16(u8 v, size_t vl) { 
            return __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vzext_vf2_u16m2(v, vl)); 
        }
        static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m2(a, b, vl); }
        static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m2(a, b, vl); }
        static i16 vsll1_i(i16 v, size_t vl) { return __riscv_vsll_vx_i16m2(v, 1, vl); }
        static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m2(p, v, vl); }
    };

    // ---- LMUL = 2 -------------------------------------------------------------
    // u8=m2, i16=m4 (widened)
    template <>
    struct rvv_traits<2>
    {
        using u8 = vuint8m2_t;
        using i8 = vint8m2_t;
        using i16 = vint16m4_t; // widened: m2 * 2 = m4

        static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m2(n); }

        static u8 vle8_u(const uint8_t *p, size_t vl) { return __riscv_vle8_v_u8m2(p, vl); }
        static u8 vslide1up_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1up_vx_u8m2(v, fill, vl); }
        static u8 vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m2(v, fill, vl); }
        static uint8_t vmv_x_u(u8 v) { return __riscv_vmv_x_s_u8m2_u8(v); }

        // Zero-extend u8 to u16, then reinterpret as signed i16 for gradient math
        static i16 vzext_i16(u8 v, size_t vl) { 
            return __riscv_vreinterpret_v_u16m4_i16m4(__riscv_vzext_vf2_u16m4(v, vl)); 
        }
        static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m4(a, b, vl); }
        static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m4(a, b, vl); }
        static i16 vsll1_i(i16 v, size_t vl) { return __riscv_vsll_vx_i16m4(v, 1, vl); }
        static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m4(p, v, vl); }
    };

    // ---- LMUL = 4 -------------------------------------------------------------
    // u8=m4, i16=m8 (widened — hardware maximum)
    template <>
    struct rvv_traits<4>
    {
        using u8 = vuint8m4_t;
        using i8 = vint8m4_t;
        using i16 = vint16m8_t; // widened: m4 * 2 = m8

        static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m4(n); }

        static u8 vle8_u(const uint8_t *p, size_t vl) { return __riscv_vle8_v_u8m4(p, vl); }
        static u8 vslide1up_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1up_vx_u8m4(v, fill, vl); }
        static u8 vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m4(v, fill, vl); }
        static uint8_t vmv_x_u(u8 v) { return __riscv_vmv_x_s_u8m4_u8(v); }

        // Zero-extend u8 to u16, then reinterpret as signed i16 for gradient math
        static i16 vzext_i16(u8 v, size_t vl) { 
            return __riscv_vreinterpret_v_u16m8_i16m8(__riscv_vzext_vf2_u16m8(v, vl)); 
        }
        static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m8(a, b, vl); }
        static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m8(a, b, vl); }
        static i16 vsll1_i(i16 v, size_t vl) { return __riscv_vsll_vx_i16m8(v, 1, vl); }
        static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m8(p, v, vl); }
    };

    template <int LMUL>
    static void make_neighbours(
        typename rvv_traits<LMUL>::u8 v,
        typename rvv_traits<LMUL>::u8 &left,
        typename rvv_traits<LMUL>::u8 &right,
        uint8_t first,
        uint8_t last,
        size_t vl)
    {
        using T = rvv_traits<LMUL>;
        
        // left neighbour: slide vector up by 1, fill position 0 with 'first'
        left = T::vslide1up_u(v, first, vl);

        // right neighbour: slide vector down by 1, fill last position with 'last'
        right = T::vslide1down_u(v, last, vl); 
    }

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
            const uint8_t *row_mid = buf + y * W;
            const uint8_t *row_bot = buf + y_bot * W;

            int16_t *out_gx = gx + y * W;
            int16_t *out_gy = gy + y * W;

            int32_t x = 0;
            size_t vl;

            for (; x < W; x += static_cast<int32_t>(vl))
            {
                vl = T::setvl_e8(W - x);

                // --- Load rows ---
                typename T::u8 v_tm = T::vle8_u(row_top + x, vl); // top-mid (center of top row)
                typename T::u8 v_mm = T::vle8_u(row_mid + x, vl); // mid-mid
                typename T::u8 v_bm = T::vle8_u(row_bot + x, vl); // bot-mid

                // --- Produce left/right neighbours with replicate padding ---
                typename T::u8 v_tl, v_tr;
                typename T::u8 v_ml, v_mr;
                typename T::u8 v_bl, v_br;

                // Grab the first and last elements for padding from memory directly
                uint8_t tm_first = row_top[x], tm_last = row_top[x + vl - 1];
                uint8_t mm_first = row_mid[x], mm_last = row_mid[x + vl - 1];
                uint8_t bm_first = row_bot[x], bm_last = row_bot[x + vl - 1];

                make_neighbours<LMUL>(v_tm, v_tl, v_tr, tm_first, tm_last, vl);
                make_neighbours<LMUL>(v_mm, v_ml, v_mr, mm_first, mm_last, vl);
                make_neighbours<LMUL>(v_bm, v_bl, v_br, bm_first, bm_last, vl);

                // Fix left boundary: if this chunk starts at x > 0, the true left neighbour
                // is the previous chunk's last element — load it directly.
                if (x > 0)
                {
                    v_tl = T::vslide1up_u(v_tm, row_top[x - 1], vl);
                    v_ml = T::vslide1up_u(v_mm, row_mid[x - 1], vl);
                    v_bl = T::vslide1up_u(v_bm, row_bot[x - 1], vl);
                }

                // Fix right boundary: if this chunk ends before W, the true right
                // neighbour is the next element in the row — provide it as the slide fill.
                if (x + static_cast<int32_t>(vl) < W)
                {
                    v_tr = T::vslide1down_u(v_tm, row_top[x + vl], vl);
                    v_mr = T::vslide1down_u(v_mm, row_mid[x + vl], vl);
                    v_br = T::vslide1down_u(v_bm, row_bot[x + vl], vl);
                }

                // --- Widen u8 -> i16 --- (Now uses Zero-Extension)
                typename T::i16 i_tl = T::vzext_i16(v_tl, vl);
                typename T::i16 i_tr = T::vzext_i16(v_tr, vl);
                typename T::i16 i_tm = T::vzext_i16(v_tm, vl);
                typename T::i16 i_ml = T::vzext_i16(v_ml, vl);
                typename T::i16 i_mr = T::vzext_i16(v_mr, vl);
                typename T::i16 i_bl = T::vzext_i16(v_bl, vl);
                typename T::i16 i_br = T::vzext_i16(v_br, vl);
                typename T::i16 i_bm = T::vzext_i16(v_bm, vl);

                // --- Gx = (tr - tl) + 2*(mr - ml) + (br - bl) ---
                typename T::i16 gx_top = T::vsub_i(i_tr, i_tl, vl);
                typename T::i16 gx_mid = T::vsll1_i(T::vsub_i(i_mr, i_ml, vl), vl); // *2
                typename T::i16 gx_bot = T::vsub_i(i_br, i_bl, vl);
                typename T::i16 v_gx = T::vadd_i(T::vadd_i(gx_top, gx_mid, vl), gx_bot, vl);

                // --- Gy = (bl + 2*bm + br) - (tl + 2*tm + tr) ---
                typename T::i16 gy_bot = T::vadd_i(T::vadd_i(i_bl, T::vsll1_i(i_bm, vl), vl), i_br, vl);
                typename T::i16 gy_top = T::vadd_i(T::vadd_i(i_tl, T::vsll1_i(i_tm, vl), vl), i_tr, vl);
                typename T::i16 v_gy = T::vsub_i(gy_bot, gy_top, vl);

                // --- Store ---
                T::vse16_i(out_gx + x, v_gx, vl);
                T::vse16_i(out_gy + x, v_gy, vl);
            }
        }

        return Status::E_OK;
    }

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