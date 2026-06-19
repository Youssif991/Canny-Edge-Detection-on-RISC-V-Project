/**
 * @file    magnitude_rvv.cpp
 * @brief   Gradient magnitude L1 — RVV vectorized, fixed LMUL=4.
 *
 * Pass 1: |Gx| + |Gy| at i16/u16 LMUL=4, track max via vredmax (always m1).
 * Pass 2: normalize — widen u16->u32 (LMUL=8), scale x255, divide by max,
 *         narrow u32->u16->u8, store.
 *
 * IMPORTANT: vredmax destination is always vuint16m1_t — ISA rule.
 */

#include "magnitude_rvv.hpp"
#ifdef __riscv_vector
#include <riscv_vector.h>
#endif
#include <memory>

namespace processing
{
#ifdef __riscv_vector

struct rvv_traits_mag
{
    using i16 = vint16m4_t;
    using u16 = vuint16m4_t;
    using u32 = vuint32m8_t;  // m4 * 2 = m8
    using u8  = vuint8m2_t;   // m4 / 2 = m2

    static size_t setvl_e16(size_t n) { return __riscv_vsetvl_e16m4(n); }

    static i16  vle16_i(const int16_t  *p, size_t vl) { return __riscv_vle16_v_i16m4(p, vl); }
    static u16  vle16_u(const uint16_t *p, size_t vl) { return __riscv_vle16_v_u16m4(p, vl); }
    static i16  vneg_i(i16 v, size_t vl)              { return __riscv_vneg_v_i16m4(v, vl); }
    static i16  vmax_i(i16 a, i16 b, size_t vl)       { return __riscv_vmax_vv_i16m4(a, b, vl); }
    static u16  reinterpret(i16 v)                     { return __riscv_vreinterpret_v_i16m4_u16m4(v); }
    static u16  vadd_u(u16 a, u16 b, size_t vl)       { return __riscv_vadd_vv_u16m4(a, b, vl); }
    static void vse16_u(uint16_t *p, u16 v, size_t vl){ __riscv_vse16_v_u16m4(p, v, vl); }

    // Reduction dest is always m1 — ISA rule
    static vuint16m1_t vredmax(u16 v, vuint16m1_t acc, size_t vl)
    {
        return __riscv_vredmax_vs_u16m4_u16m1(v, acc, vl);
    }

    static u32  vwmulu_u(u16 v, uint16_t s, size_t vl) { return __riscv_vwmulu_vx_u32m8(v, s, vl); }
    static u32  vdivu_u(u32 v, uint32_t s, size_t vl)  { return __riscv_vdivu_vx_u32m8(v, s, vl); }
    static u16  vncvt_u32_u16(u32 v, size_t vl)        { return __riscv_vncvt_x_x_w_u16m4(v, vl); }
    static u8   vncvt_u16_u8(u16 v, size_t vl)         { return __riscv_vncvt_x_x_w_u8m2(v, vl); }
    static void vse8_u(uint8_t *p, u8 v, size_t vl)    { __riscv_vse8_v_u8m2(p, v, vl); }
};

using T = rvv_traits_mag;

template <>
Status MagL1<4>(const image::io::metadata_t<uint8_t> &image,
                const int16_t *__restrict Gx,
                const int16_t *__restrict Gy)
{
    if (!image.height || !image.width || !image.buffer)
        return image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;

    auto mag_raw = static_cast<uint16_t *>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(image.pixel_count * sizeof(uint16_t))));
    if (!mag_raw)
        return Status::E_ALLOC_FAIL;

    std::unique_ptr<uint16_t[], utils::memory::deleter> magnitude_buffer(mag_raw);

    uint16_t    max_magnitude   = 0;
    vuint16m1_t v_max_magnitude = __riscv_vmv_v_x_u16m1(0, 1); // reduction seed — always m1
    const uint32_t pixel_count  = image.pixel_count;
    size_t vl;

    // --- Pass 1: |Gx| + |Gy|, track running max ---
    for (uint32_t i = 0; i < pixel_count; i += vl)
    {
        vl = T::setvl_e16(pixel_count - i);

        T::i16 v_gx     = T::vle16_i(Gx + i, vl);
        T::i16 v_abs_gx = T::vmax_i(v_gx, T::vneg_i(v_gx, vl), vl);
        T::u16 v_u_gx   = T::reinterpret(v_abs_gx);

        T::i16 v_gy     = T::vle16_i(Gy + i, vl);
        T::i16 v_abs_gy = T::vmax_i(v_gy, T::vneg_i(v_gy, vl), vl);
        T::u16 v_u_gy   = T::reinterpret(v_abs_gy);

        T::u16 v_raw_mag = T::vadd_u(v_u_gx, v_u_gy, vl);
        T::vse16_u(magnitude_buffer.get() + i, v_raw_mag, vl);

        v_max_magnitude = T::vredmax(v_raw_mag, v_max_magnitude, vl);
    }

    max_magnitude = __riscv_vmv_x_s_u16m1_u16(v_max_magnitude);

    if (max_magnitude == 0)
    {
        std::fill(image.buffer.get(), image.buffer.get() + pixel_count, uint8_t{0});
        return Status::E_OK;
    }

    // --- Pass 2: normalize to [0, 255] ---
    for (uint32_t i = 0; i < pixel_count; i += vl)
    {
        vl = T::setvl_e16(pixel_count - i);

        T::u16 v_mag     = T::vle16_u(magnitude_buffer.get() + i, vl);
        T::u32 v_scaled  = T::vwmulu_u(v_mag, 255, vl);
        v_scaled         = T::vdivu_u(v_scaled, max_magnitude, vl);
        T::u16 v_narrow1 = T::vncvt_u32_u16(v_scaled, vl);
        T::u8  v_narrow2 = T::vncvt_u16_u8(v_narrow1, vl);

        T::vse8_u(image.buffer.get() + i, v_narrow2, vl);
    }

    return Status::E_OK;
}

#endif // __riscv_vector

#ifndef __riscv_vector
template <>
Status MagL1<4>(const image::io::metadata_t<uint8_t> &,
                const int16_t *__restrict,
                const int16_t *__restrict)
{
    return Status::E_NOK;
}
#endif

} // namespace processing