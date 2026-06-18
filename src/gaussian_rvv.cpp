/**
 * @file    gaussian_rvv.cpp
 * @brief   Vectorized Gaussian blur implementations for image processing using RVV.
 */

#include "gaussian_rvv.hpp"
#include "utils.hpp"
#ifdef __riscv
#include <riscv_vector.h>
#endif
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace processing
{

#ifdef __riscv
namespace
{

template <typename PixelT>
Status allocate_and_pad_image(
    const image::io::metadata_t<PixelT>& input_image,
    int32_t kernel_radius,
    std::unique_ptr<PixelT[], utils::memory::deleter>& padded_image,
    uint32_t& padded_width,
    uint32_t& padded_height)
{
    const int32_t image_width  = static_cast<int32_t>(input_image.width);
    const int32_t image_height = static_cast<int32_t>(input_image.height);

    padded_width  = image_width  + 2 * kernel_radius;
    padded_height = image_height + 2 * kernel_radius;
    const uint32_t padded_size = padded_width * padded_height;

    auto raw_ptr = static_cast<PixelT*>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(padded_size * sizeof(PixelT))));
    if (!raw_ptr)
    {
        return Status::E_ALLOC_FAIL;
    }

    padded_image.reset(raw_ptr);
    std::fill(padded_image.get(), padded_image.get() + padded_size, PixelT{0});

    for (int32_t r = 0; r < image_height; ++r)
    {
        std::copy_n(
            &input_image.buffer.get()[r * image_width],
            image_width,
            &padded_image.get()[(r + kernel_radius) * padded_width + kernel_radius]);
    }

    return Status::E_OK;
}

} // namespace
#endif

Status gaussian_spatial_5x5_rvv(image::io::metadata_t<uint8_t>& input_image)
{
#ifdef __riscv
    if (!input_image.height || !input_image.width || !input_image.buffer)
    {
        return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
    }

    const int32_t image_width   = static_cast<int32_t>(input_image.width);
    const int32_t image_height  = static_cast<int32_t>(input_image.height);
    const int32_t kernel_radius = 2;

    uint32_t padded_width = 0;
    uint32_t padded_height = 0;
    std::unique_ptr<uint8_t[], utils::memory::deleter> padded_image;

    Status status = allocate_and_pad_image(input_image, kernel_radius, padded_image, padded_width, padded_height);
    if (status != Status::E_OK)
    {
        return status;
    }

    auto output_image_raw = static_cast<uint8_t*>(
        utils::memory::aligned_alloc(64, input_image.aligned_buffer_size));
    if (!output_image_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<uint8_t[], utils::memory::deleter> output_image(output_image_raw);

    const uint8_t* __restrict padded_ptr = padded_image.get();
    uint8_t* __restrict out_ptr = output_image.get();

    for (int32_t row_index = 0; row_index < image_height; ++row_index)
    {
        for (int32_t col_index = 0; col_index < image_width; )
        {
            size_t vl = __riscv_vsetvl_e8m1(image_width - col_index);
            vint32m4_t acc = __riscv_vmv_v_x_i32m4(0, vl);

            for (int32_t kr = -kernel_radius; kr <= kernel_radius; ++kr)
            {
                for (int32_t kc = -kernel_radius; kc <= kernel_radius; ++kc)
                {
                    int16_t weight = kernels::GAUSSIAN_5X5[kr + kernel_radius][kc + kernel_radius];
                    
                    uint32_t p_row = row_index + kernel_radius + kr;
                    uint32_t p_col = col_index + kernel_radius + kc;
                    
                    vuint8m1_t px8 = __riscv_vle8_v_u8m1(&padded_ptr[p_row * padded_width + p_col], vl);
                    vuint16m2_t px16u = __riscv_vzext_vf2_u16m2(px8, vl);
                    vint16m2_t px16s = __riscv_vreinterpret_v_u16m2_i16m2(px16u);
                    
                    acc = __riscv_vwmacc_vx_i32m4(acc, weight, px16s, vl);
                }
            }

            // Normalization: sum / 273 approximated by (sum * 240) >> 16
            acc = __riscv_vmul_vx_i32m4(acc, 240, vl);
            acc = __riscv_vsra_vx_i32m4(acc, 16, vl);

            // Clamp to [0, 255]
            acc = __riscv_vmax_vx_i32m4(acc, 0, vl);
            acc = __riscv_vmin_vx_i32m4(acc, 255, vl);

            // Narrow down to uint8_t
            vint16m2_t narrow16 = __riscv_vncvt_x_x_w_i16m2(acc, vl);
            vint8m1_t narrow8 = __riscv_vncvt_x_x_w_i8m1(narrow16, vl);
            vuint8m1_t out_px8 = __riscv_vreinterpret_v_i8m1_u8m1(narrow8);

            __riscv_vse8_v_u8m1(&out_ptr[row_index * image_width + col_index], out_px8, vl);

            col_index += vl;
        }
    }

    input_image.buffer = std::move(output_image);

    return Status::E_OK;
#else
    (void)input_image;
    return Status::E_NOK; // Not supported on host
#endif
}

} // namespace processing
