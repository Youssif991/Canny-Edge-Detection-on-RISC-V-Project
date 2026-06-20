/**
 * @file    gaussian_rvv.cpp
 * @brief   Gaussian blur — RVV vectorized, fixed LMUL=2.
 */

#include "gaussian_rvv.hpp"
#include "gaussian.hpp"
#include "utils.hpp"
#ifdef __riscv_vector
#include <riscv_vector.h>
#endif
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace processing
{

#ifdef __riscv_vector
    namespace
    {

        struct rvv_traits
        {
            using u8 = vuint8m2_t;
            using u16 = vuint16m4_t;
            using u32 = vuint32m8_t;

            static size_t setvl_e8(size_t n) { return __riscv_vsetvl_e8m2(n); }
            static size_t setvl_e32(size_t n) { return __riscv_vsetvl_e32m8(n); }

            static u8 vle8(const uint8_t *p, size_t vl) { return __riscv_vle8_v_u8m2(p, vl); }
            static u32 vmv0(size_t vl) { return __riscv_vmv_v_x_u32m8(0, vl); }
            static u16 vzext(u8 v, size_t vl) { return __riscv_vzext_vf2_u16m4(v, vl); }

            static u32 vwmaccu(u32 acc, uint8_t s, u16 v, size_t vl)
            {
                return __riscv_vwmaccu_vx_u32m8(acc, s, v, vl);
            }

            static u32 vmul(u32 v, uint32_t s, size_t vl) { return __riscv_vmul_vx_u32m8(v, s, vl); }
            static u32 vsrl(u32 v, uint32_t s, size_t vl) { return __riscv_vsrl_vx_u32m8(v, s, vl); }

            static u8 narrow_u32_to_u8(u32 v, size_t vl)
            {
                return __riscv_vncvt_x_x_w_u8m2(__riscv_vncvt_x_x_w_u16m4(v, vl), vl);
            }

            static void vse8(uint8_t *p, u8 v, size_t vl) { __riscv_vse8_v_u8m2(p, v, vl); }
        };

    } // namespace

    Status gaussian_spatial_5x5_rvv(image::io::metadata_t<uint8_t> &input_image)
    {
        if (!input_image.height || !input_image.width || !input_image.buffer)
            return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;

        const int32_t image_width = static_cast<int32_t>(input_image.width);
        const int32_t image_height = static_cast<int32_t>(input_image.height);
        const int32_t kernel_radius = 2;

        // ── Padding, inlined directly (matches gaussian.cpp's style) ──
        const uint32_t padded_image_width = image_width + 2 * kernel_radius;
        const uint32_t padded_image_height = image_height + 2 * kernel_radius;
        const uint32_t padded_image_size = padded_image_width * padded_image_height;

        auto padded_image_raw = static_cast<uint8_t *>(
            utils::memory::aligned_alloc(64,
                                         utils::memory::align_64(padded_image_size * sizeof(uint8_t))));
        if (!padded_image_raw)
        {
            return Status::E_ALLOC_FAIL;
        }
        std::unique_ptr<uint8_t[], utils::memory::deleter> padded_image(padded_image_raw);
        std::fill(padded_image.get(), padded_image.get() + padded_image_size, uint8_t{0});

        for (int32_t row_index = 0; row_index < image_height; ++row_index)
        {
            std::copy_n(
                &input_image.buffer.get()[row_index * image_width],
                image_width,
                &padded_image.get()[(row_index + kernel_radius) * padded_image_width + kernel_radius]);
        }

        auto output_raw = static_cast<uint8_t *>(
            utils::memory::aligned_alloc(64, input_image.aligned_buffer_size));
        if (!output_raw)
            return Status::E_ALLOC_FAIL;
        std::unique_ptr<uint8_t[], utils::memory::deleter> output_image(output_raw);

        const uint8_t *__restrict padded_ptr = padded_image.get();
        uint8_t *__restrict out_ptr = output_image.get();

        using T = rvv_traits;

        for (int32_t y = 0; y < image_height; ++y)
        {
            for (int32_t x = 0; x < image_width;)
            {
                size_t vl = T::setvl_e8(image_width - x);
                auto sum = T::vmv0(vl);

                for (int32_t ky = -kernel_radius; ky <= kernel_radius; ++ky)
                {
                    const uint32_t row_off = (y + kernel_radius + ky) * padded_image_width;
                    for (int32_t kx = -kernel_radius; kx <= kernel_radius; ++kx)
                    {
                        uint8_t weight = static_cast<uint8_t>(
                            kernels::GAUSSIAN_5X5[ky + kernel_radius][kx + kernel_radius]);

                        auto px = T::vle8(&padded_ptr[row_off + (x + kernel_radius + kx)], vl);
                        auto px16 = T::vzext(px, vl);
                        sum = T::vwmaccu(sum, weight, px16, vl);
                    }
                }

                sum = T::vmul(sum, 240, vl);
                sum = T::vsrl(sum, 16, vl);
                T::vse8(&out_ptr[y * image_width + x], T::narrow_u32_to_u8(sum, vl), vl);
                x += static_cast<int32_t>(vl);
            }
        }

        input_image.buffer = std::move(output_image);
        return Status::E_OK;
    }

#endif // __riscv_vector
} // namespace processing