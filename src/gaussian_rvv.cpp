#include "gaussian_rvv.hpp"
#include "gaussian.hpp"
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
        const image::io::metadata_t<PixelT> &input_image,
        int32_t kernel_radius,
        std::unique_ptr<PixelT[], utils::memory::deleter> &padded_image,
        uint32_t &padded_width,
        uint32_t &padded_height)
    {
        const int32_t image_width = static_cast<int32_t>(input_image.width);
        const int32_t image_height = static_cast<int32_t>(input_image.height);

        padded_width = image_width + 2 * kernel_radius;
        padded_height = image_height + 2 * kernel_radius;
        const uint32_t padded_size = padded_width * padded_height;

        auto raw_ptr = static_cast<PixelT *>(
            utils::memory::aligned_alloc(64, utils::memory::align_64(padded_size * sizeof(PixelT))));
        if (!raw_ptr) return Status::E_ALLOC_FAIL;

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

    // ---------------------------------------------------------
    // RVV Traits Definition
    // ---------------------------------------------------------
    template <int LMUL> struct rvv_traits;

    template <> struct rvv_traits<1>
    {
        using u8  = vuint8m1_t;
        using u16 = vuint16m2_t;
        using u32 = vuint32m4_t;

        static size_t setvl_e8 (size_t n) { return __riscv_vsetvl_e8m1(n);  }
        static size_t setvl_e32(size_t n) { return __riscv_vsetvl_e32m4(n); }

        static u8  vle8 (const uint8_t* p, size_t vl) { return __riscv_vle8_v_u8m1(p, vl);       }
        static u32 vmv0 (size_t vl)                    { return __riscv_vmv_v_x_u32m4(0, vl);      }
        static u16 vzext(u8 v, size_t vl)              { return __riscv_vzext_vf2_u16m2(v, vl);    }

        static u32 vwmaccu(u32 acc, uint8_t s, u16 v, size_t vl)
        { return __riscv_vwmaccu_vx_u32m4(acc, s, v, vl); }

        static u32 vmacc(u32 acc, uint8_t s, u32 v, size_t vl)
        { return __riscv_vmacc_vx_u32m4(acc, s, v, vl); }

        static u32 vmul(u32 v, uint32_t s, size_t vl) { return __riscv_vmul_vx_u32m4(v, s, vl);  }
        static u32 vsrl(u32 v, uint32_t s, size_t vl) { return __riscv_vsrl_vx_u32m4(v, s, vl);  }

        static u8 narrow_u32_to_u8(u32 v, size_t vl)
        { return __riscv_vncvt_x_x_w_u8m1(__riscv_vncvt_x_x_w_u16m2(v, vl), vl); }

        static void vse8 (uint8_t*  p, u8  v, size_t vl) { __riscv_vse8_v_u8m1(p, v, vl);   }
        static void vse32(uint32_t* p, u32 v, size_t vl) { __riscv_vse32_v_u32m4(p, v, vl); }

        static u32 vle32(const uint32_t* p, size_t vl) { return __riscv_vle32_v_u32m4(p, vl); }
    };

    template <> struct rvv_traits<2>
    {
        using u8  = vuint8m2_t;
        using u16 = vuint16m4_t;
        using u32 = vuint32m8_t;

        static size_t setvl_e8 (size_t n) { return __riscv_vsetvl_e8m2(n);  }
        static size_t setvl_e32(size_t n) { return __riscv_vsetvl_e32m8(n); }

        static u8  vle8 (const uint8_t* p, size_t vl) { return __riscv_vle8_v_u8m2(p, vl);       }
        static u32 vmv0 (size_t vl)                    { return __riscv_vmv_v_x_u32m8(0, vl);      }
        static u16 vzext(u8 v, size_t vl)              { return __riscv_vzext_vf2_u16m4(v, vl);    }

        static u32 vwmaccu(u32 acc, uint8_t s, u16 v, size_t vl)
        { return __riscv_vwmaccu_vx_u32m8(acc, s, v, vl); }

        static u32 vmacc(u32 acc, uint8_t s, u32 v, size_t vl)
        { return __riscv_vmacc_vx_u32m8(acc, s, v, vl); }

        static u32 vmul(u32 v, uint32_t s, size_t vl) { return __riscv_vmul_vx_u32m8(v, s, vl);  }
        static u32 vsrl(u32 v, uint32_t s, size_t vl) { return __riscv_vsrl_vx_u32m8(v, s, vl);  }

        static u8 narrow_u32_to_u8(u32 v, size_t vl)
        { return __riscv_vncvt_x_x_w_u8m2(__riscv_vncvt_x_x_w_u16m4(v, vl), vl); }

        static void vse8 (uint8_t*  p, u8  v, size_t vl) { __riscv_vse8_v_u8m2(p, v, vl);   }
        static void vse32(uint32_t* p, u32 v, size_t vl) { __riscv_vse32_v_u32m8(p, v, vl); }

        static u32 vle32(const uint32_t* p, size_t vl) { return __riscv_vle32_v_u32m8(p, vl); }
    };

    template <> struct rvv_traits<4>
    {
        // u8m4 -> u16m8 -> u32: widening to m16 is invalid.
        // Accumulator is capped at m8; pixel LMUL is 4, accumulator LMUL is 8.
        using u8  = vuint8m2_t;
        using u16 = vuint16m4_t;
        using u32 = vuint32m8_t;

        static size_t setvl_e8 (size_t n) { return __riscv_vsetvl_e8m2(n);  }
        static size_t setvl_e32(size_t n) { return __riscv_vsetvl_e32m8(n); }

        static u8  vle8 (const uint8_t* p, size_t vl) { return __riscv_vle8_v_u8m2(p, vl);       }
        static u32 vmv0 (size_t vl)                    { return __riscv_vmv_v_x_u32m8(0, vl);      }
        static u16 vzext(u8 v, size_t vl)              { return __riscv_vzext_vf2_u16m4(v, vl);    }

        static u32 vwmaccu(u32 acc, uint8_t s, u16 v, size_t vl)
        { return __riscv_vwmaccu_vx_u32m8(acc, s, v, vl); }

        static u32 vmacc(u32 acc, uint8_t s, u32 v, size_t vl)
        { return __riscv_vmacc_vx_u32m8(acc, s, v, vl); }

        static u32 vmul(u32 v, uint32_t s, size_t vl) { return __riscv_vmul_vx_u32m8(v, s, vl);  }
        static u32 vsrl(u32 v, uint32_t s, size_t vl) { return __riscv_vsrl_vx_u32m8(v, s, vl);  }

        static u8 narrow_u32_to_u8(u32 v, size_t vl)
        { return __riscv_vncvt_x_x_w_u8m2(__riscv_vncvt_x_x_w_u16m4(v, vl), vl); }

        static void vse8 (uint8_t*  p, u8  v, size_t vl) { __riscv_vse8_v_u8m2(p, v, vl);   }
        static void vse32(uint32_t* p, u32 v, size_t vl) { __riscv_vse32_v_u32m8(p, v, vl); }

        static u32 vle32(const uint32_t* p, size_t vl) { return __riscv_vle32_v_u32m8(p, vl); }
    };

    // ---------------------------------------------------------
    // Core Logic Template
    // ---------------------------------------------------------
    template <int LMUL>
    Status gaussian_spatial_5x5_rvv_impl(image::io::metadata_t<uint8_t> &input_image)
    {
        if (!input_image.height || !input_image.width || !input_image.buffer)
        {
            return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
        }

        const int32_t image_width = static_cast<int32_t>(input_image.width);
        const int32_t image_height = static_cast<int32_t>(input_image.height);
        const int32_t kernel_radius = 2;

        const uint32_t pw = image_width + 2 * kernel_radius;
        uint32_t padded_width = 0;
        uint32_t padded_height = 0;
        std::unique_ptr<uint8_t[], utils::memory::deleter> padded_image;

        Status status = allocate_and_pad_image(input_image, kernel_radius, padded_image, padded_width, padded_height);
        if (status != Status::E_OK) return status;

        auto output_image_raw = static_cast<uint8_t *>(utils::memory::aligned_alloc(64, input_image.aligned_buffer_size));
        if (!output_image_raw) return Status::E_ALLOC_FAIL;
        std::unique_ptr<uint8_t[], utils::memory::deleter> output_image(output_image_raw);

        const uint8_t *__restrict padded_ptr = padded_image.get();
        uint8_t *__restrict out_ptr = output_image.get();

        using T = rvv_traits<LMUL>;

        for (int32_t y = 0; y < image_height; ++y)
        {
            for (int32_t x = 0; x < image_width;)
            {
                size_t vl = T::setvl_e8(image_width - x);
                auto sum = T::vmv0(vl);

                for (int32_t ky = -kernel_radius; ky <= kernel_radius; ++ky)
                {
                    const uint32_t row_off = (y + kernel_radius + ky) * pw;
                    for (int32_t kx = -kernel_radius; kx <= kernel_radius; ++kx)
                    {
                        uint8_t weight = static_cast<uint8_t>(kernels::GAUSSIAN_5X5[ky + kernel_radius][kx + kernel_radius]);

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

} // namespace
#endif // __riscv

    Status gaussian_spatial_5x5_rvv_lmul1(image::io::metadata_t<uint8_t> &input_image)
    {
#ifdef __riscv
        return gaussian_spatial_5x5_rvv_impl<1>(input_image);
#else
        (void)input_image;
        return Status::E_NOK;
#endif
    }

    Status gaussian_spatial_5x5_rvv_lmul2(image::io::metadata_t<uint8_t> &input_image)
    {
#ifdef __riscv
        return gaussian_spatial_5x5_rvv_impl<2>(input_image);
#else
        (void)input_image;
        return Status::E_NOK;
#endif
    }

    Status gaussian_spatial_5x5_rvv_lmul4(image::io::metadata_t<uint8_t> &input_image)
    {
#ifdef __riscv
        return gaussian_spatial_5x5_rvv_impl<4>(input_image);
#else
        (void)input_image;
        return Status::E_NOK;
#endif
    }

} // namespace processing
