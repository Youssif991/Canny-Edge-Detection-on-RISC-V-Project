/**
 * @file    sobel_rvv.cpp
 * @brief   Sobel gradient computation — RVV vectorized, fixed LMUL=2.
 *
 * Gx = (tr - tl) + 2*(mr - ml) + (br - bl)
 * Gy = (bl + 2*bm + br) - (tl + 2*tm + tr)
 *
 * Pixels load as u8 at LMUL=2. After vzext (u8->u16 zero-extend, reinterpreted
 * as i16) the gradient arithmetic runs at LMUL=4. Stores i16 directly.
 */

#include "sobel_rvv.hpp"
#ifdef __riscv_vector
#include <riscv_vector.h>
#endif
#include <algorithm>

namespace processing
{
#ifdef __riscv_vector
    struct rvv_traits_sobel
    {
        using u8 = vuint8m2_t;
        using i16 = vint16m4_t;

        static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m2(n); }

        static u8 vle8_u(const uint8_t *p, size_t vl) { return __riscv_vle8_v_u8m2(p, vl); }
        static u8 vslide1up_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1up_vx_u8m2(v, fill, vl); }
        static u8 vslide1down_u(u8 v, uint8_t fill, size_t vl) { return __riscv_vslide1down_vx_u8m2(v, fill, vl); }

        // Zero-extend u8->u16 then reinterpret as i16
        static i16 vzext_i16(u8 v, size_t vl)
        {
            return __riscv_vreinterpret_v_u16m4_i16m4(__riscv_vzext_vf2_u16m4(v, vl));
        }

        static i16 vadd_i(i16 a, i16 b, size_t vl) { return __riscv_vadd_vv_i16m4(a, b, vl); }
        static i16 vsub_i(i16 a, i16 b, size_t vl) { return __riscv_vsub_vv_i16m4(a, b, vl); }
        static i16 vsll1_i(i16 v, size_t vl) { return __riscv_vsll_vx_i16m4(v, 1, vl); }
        static void vse16_i(int16_t *p, i16 v, size_t vl) { __riscv_vse16_v_i16m4(p, v, vl); }
    };

    using T = rvv_traits_sobel;

    static void make_neighbours(
        T::u8 v,
        T::u8 &left, T::u8 &right,
        uint8_t fill_left, uint8_t fill_right,
        size_t vl)
    {
        left = T::vslide1up_u(v, fill_left, vl);
        right = T::vslide1down_u(v, fill_right, vl);
    }

    template <>
    Status sobel_3x3_rvv<2>(const image::io::metadata_t<uint8_t> &input,
                            int16_t *__restrict gx,
                            int16_t *__restrict gy)
    {
        if (!input.height || !input.width || !input.buffer || !gx || !gy)
            return input.buffer ? Status::E_NOK : Status::E_INVAL_PTR;

        const int32_t W = static_cast<int32_t>(input.width);
        const int32_t H = static_cast<int32_t>(input.height);
        const uint8_t *buf = input.buffer.get();

        for (int32_t y = 0; y < H; ++y)
        {
            const int32_t y_top = (y == 0) ? 0 : y - 1;
            const int32_t y_bot = (y == H - 1) ? H - 1 : y + 1;

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

                T::u8 v_tm = T::vle8_u(row_top + x, vl);
                T::u8 v_mm = T::vle8_u(row_mid + x, vl);
                T::u8 v_bm = T::vle8_u(row_bot + x, vl);

                // Default fill: replicate chunk boundary
                uint8_t tm_l = row_top[x], tm_r = row_top[x + vl - 1];
                uint8_t mm_l = row_mid[x], mm_r = row_mid[x + vl - 1];
                uint8_t bm_l = row_bot[x], bm_r = row_bot[x + vl - 1];

                // True neighbours from memory when they exist
                if (x > 0)
                {
                    tm_l = row_top[x - 1];
                    mm_l = row_mid[x - 1];
                    bm_l = row_bot[x - 1];
                }
                if (x + static_cast<int32_t>(vl) < W)
                {
                    tm_r = row_top[x + vl];
                    mm_r = row_mid[x + vl];
                    bm_r = row_bot[x + vl];
                }

                T::u8 v_tl, v_tr, v_ml, v_mr, v_bl, v_br;
                make_neighbours(v_tm, v_tl, v_tr, tm_l, tm_r, vl);
                make_neighbours(v_mm, v_ml, v_mr, mm_l, mm_r, vl);
                make_neighbours(v_bm, v_bl, v_br, bm_l, bm_r, vl);

                T::i16 i_tl = T::vzext_i16(v_tl, vl);
                T::i16 i_tr = T::vzext_i16(v_tr, vl);
                T::i16 i_tm = T::vzext_i16(v_tm, vl);
                T::i16 i_ml = T::vzext_i16(v_ml, vl);
                T::i16 i_mr = T::vzext_i16(v_mr, vl);
                T::i16 i_bl = T::vzext_i16(v_bl, vl);
                T::i16 i_br = T::vzext_i16(v_br, vl);
                T::i16 i_bm = T::vzext_i16(v_bm, vl);

                // Gx = (tr-tl) + 2*(mr-ml) + (br-bl)
                T::i16 v_gx = T::vadd_i(
                    T::vadd_i(T::vsub_i(i_tr, i_tl, vl),
                              T::vsll1_i(T::vsub_i(i_mr, i_ml, vl), vl), vl),
                    T::vsub_i(i_br, i_bl, vl), vl);

                // Gy = (bl + 2*bm + br) - (tl + 2*tm + tr)
                T::i16 gy_bot = T::vadd_i(T::vadd_i(i_bl, T::vsll1_i(i_bm, vl), vl), i_br, vl);
                T::i16 gy_top = T::vadd_i(T::vadd_i(i_tl, T::vsll1_i(i_tm, vl), vl), i_tr, vl);
                T::i16 v_gy = T::vsub_i(gy_bot, gy_top, vl);

                T::vse16_i(out_gx + x, v_gx, vl);
                T::vse16_i(out_gy + x, v_gy, vl);
            }
        }

        return Status::E_OK;
    }

#endif // __riscv_vector

#ifndef __riscv_vector
    template <>
    Status sobel_3x3_rvv<2>(const image::io::metadata_t<uint8_t> &,
                            int16_t *__restrict,
                            int16_t *__restrict)
    {
        return Status::E_NOK;
    }
#endif

} // namespace processing