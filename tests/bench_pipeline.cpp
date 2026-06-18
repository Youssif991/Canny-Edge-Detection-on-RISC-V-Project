#include "gaussian.hpp"
#include "gaussian_rvv.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "magnitude_rvv.hpp"
#include "std_types.hpp"
#include "utils.hpp"
#include "direction.hpp"

#include <iostream>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <chrono>

/* ── CHANGE THESE TO CONFIGURE THE RUN ──────────────────────────
 *
 *  LMUL_SWEEP   : RVV LMUL factor  (1 | 2 | 4) - maps to acc2, acc4, acc8
 *
 *  PIPELINE_SEL : which stage to benchmark
 *      0 → Gaussian spatial only
 *      2 → Sobel only            (uses pre-blurred input)
 *      3 → Magnitude L1 only     (uses pre-computed gx/gy)
 *      6 → Full pipeline         (all stages timed together)
 *
 *  Scalar vs RVV is controlled by the Makefile -march flag:
 *      bench-scalar-O3  →  -march=rv64gc   (__riscv undefined)
 *      bench-rvv-O3     →  -march=rv64gcv  (__riscv defined)
 *
 * ────────────────────────────────────────────────────────────── */
#define LMUL_SWEEP  4
#define PIPELINE_SEL 0

/* ── Timing helper ───────────────────────────────────────────── */
using Clock = std::chrono::high_resolution_clock;

static double elapsed_ms(Clock::time_point s, Clock::time_point e)
{
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(e - s).count();
}

/* ── Benchmark macro ─────────────────────────────────────────── */
#define BENCH(label, code)                                        \
    {                                                             \
        auto t0 = Clock::now();                                   \
        { code; }                                                 \
        auto t1 = Clock::now();                                   \
        printf("%-24s %.3f ms\n", label, elapsed_ms(t0, t1));     \
    }

/* ── Allocate a fresh metadata copy of an image ─────────────── */
static image::io::metadata_t<uint8_t> make_copy(const image::io::metadata_t<uint8_t>& src)
{
    image::io::metadata_t<uint8_t> dst;
    dst.width               = src.width;
    dst.height              = src.height;
    dst.pixel_count         = src.pixel_count;
    dst.aligned_buffer_size = src.aligned_buffer_size;
    dst.buffer.reset(static_cast<uint8_t*>(
        utils::memory::aligned_alloc(64, src.aligned_buffer_size)));
    return dst;
}

int main()
{
    // Generate dummy image data instead of load_raw to ensure QEMU test always runs without missing file errors
    constexpr uint32_t WIDTH  = 512;
    constexpr uint32_t HEIGHT = 512;
    const size_t n = WIDTH * HEIGHT;

    image::io::metadata_t<uint8_t> image;
    image.width  = WIDTH;
    image.height = HEIGHT;
    image.pixel_count = n;
    image.aligned_buffer_size = n;
    image.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, n)));

    for (size_t i = 0; i < n; ++i) {
        image.buffer[i] = static_cast<uint8_t>((i % 256));
    }

    /* ── Allocate Sobel gradient buffers ─────────────────────── */
    auto* gx = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    auto* gy = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    if (!gx || !gy) { printf("ERROR: alloc failed\n"); return 1; }

    printf("--- Benchmark (LMUL=%d, %s) ---\n",
           LMUL_SWEEP,
#if defined(__riscv)
           "RVV"
#else
           "Scalar"
#endif
    );

    /* ── Pre-compute blurred image for stages that need it ────── */
#if PIPELINE_SEL == 2 || PIPELINE_SEL == 3 || PIPELINE_SEL == 6
    {
        auto blurred = make_copy(image);
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        
#if defined(__riscv)
    #if LMUL_SWEEP == 1
        (void)processing::gaussian_spatial_5x5_rvv_lmul1(blurred);
    #elif LMUL_SWEEP == 2
        (void)processing::gaussian_spatial_5x5_rvv_lmul2(blurred);
    #elif LMUL_SWEEP == 4
        (void)processing::gaussian_spatial_5x5_rvv_lmul4(blurred);
    #endif
#else
        (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(blurred);
#endif

        /* Pre-compute gx/gy for magnitude/direction stages */
#if PIPELINE_SEL == 3
        (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
#endif

        /* ── Stage 2: Sobel only ─────────────────────────────── */
#if PIPELINE_SEL == 2
        BENCH("Sobel Filter",
            (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
        )
#endif

        /* ── Stage 3: Magnitude L1 only ──────────────────────── */
#if PIPELINE_SEL == 3
        {
            auto mag = make_copy(image);
            BENCH("Magnitude L1",
#if defined(__riscv)
                (void)processing::MagL1_rvv(mag, gx, gy);
#else
                (void)processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx, gy);
#endif
            )
        }
#endif
    }
#endif /* stages 2-5 */

    /* ── Stage 0: Gaussian spatial only ──────────────────────── */
#if PIPELINE_SEL == 0
    {
        auto img_copy = make_copy(image);
        BENCH("Gaussian spatial",
            std::copy_n(image.buffer.get(), n, img_copy.buffer.get());
#if defined(__riscv)
    #if LMUL_SWEEP == 1
            (void)processing::gaussian_spatial_5x5_rvv_lmul1(img_copy);
    #elif LMUL_SWEEP == 2
            (void)processing::gaussian_spatial_5x5_rvv_lmul2(img_copy);
    #elif LMUL_SWEEP == 4
            (void)processing::gaussian_spatial_5x5_rvv_lmul4(img_copy);
    #endif
#else
            (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_copy);
#endif
        )
    }
#endif

    /* ── Stage 6: Full pipeline ───────────────────────────────── */
#if PIPELINE_SEL == 6
    {
        auto blurred = make_copy(image);
        auto mag     = make_copy(image);

        BENCH("Full pipeline",
            std::copy_n(image.buffer.get(), n, blurred.buffer.get());
#if defined(__riscv)
    #if LMUL_SWEEP == 1
            (void)processing::gaussian_spatial_5x5_rvv_lmul1(blurred);
    #elif LMUL_SWEEP == 2
            (void)processing::gaussian_spatial_5x5_rvv_lmul2(blurred);
    #elif LMUL_SWEEP == 4
            (void)processing::gaussian_spatial_5x5_rvv_lmul4(blurred);
    #endif
            (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
            (void)processing::MagL1_rvv(mag, gx, gy);
#else
            (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(blurred);
            (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
            (void)processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx, gy);
#endif
        )
    }
#endif

    std::free(gx);
    std::free(gy);
    return 0;
}
