#include "gaussian.hpp"
#include "gaussian_rvv.hpp"
#include "sobel.hpp"
#include "sobel_rvv.hpp"
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

/* ── CHANGE THESE TO CONFIGURE THE RUN ────────────────────────── */
#define LMUL_SWEEP   4
#define PIPELINE_SEL 6

/* ── Timing helper ───────────────────────────────────────────── */
using Clock = std::chrono::high_resolution_clock;

static double elapsed_ms(Clock::time_point s, Clock::time_point e)
{
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(e - s).count();
}

/* ── Benchmark macro ─────────────────────────────────────────── */
// Wrapped 'code' in a lambda/block context execution to safely handle macro expansions inside it
#define BENCH(label, ...)                                         \
    {                                                             \
        auto t0 = Clock::now();                                   \
        { __VA_ARGS__; }                                          \
        auto t1 = Clock::now();                                   \
        printf("%-24s %.3f ms\n", label, elapsed_ms(t0, t1));    \
    }

/* ── Gaussian dispatch — only LMUL 1 and 2 exist ────────────── */
#define GAUSSIAN_RVV(img)                                             \
    do {                                                              \
        _Pragma("GCC diagnostic push")                                \
        _Pragma("GCC diagnostic ignored \"-Wunreachable-code\"")      \
        if constexpr (LMUL_SWEEP == 1)                                \
            (void)processing::gaussian_spatial_5x5_rvv_lmul1(img);    \
        else                                                          \
            (void)processing::gaussian_spatial_5x5_rvv_lmul2(img);    \
        _Pragma("GCC diagnostic pop")                                 \
    } while(0)

/* ── Sobel dispatch — LMUL 1, 2, and 4 all supported ────────── */
#define SOBEL_RVV(img, gx, gy)                                        \
    do {                                                              \
        _Pragma("GCC diagnostic push")                                \
        _Pragma("GCC diagnostic ignored \"-Wunreachable-code\"")      \
        if constexpr (LMUL_SWEEP == 1)                                \
            (void)processing::sobel_3x3_rvv<1>(img, gx, gy);          \
        else if constexpr (LMUL_SWEEP == 2)                           \
            (void)processing::sobel_3x3_rvv<2>(img, gx, gy);          \
        else                                                          \
            (void)processing::sobel_3x3_rvv<4>(img, gx, gy);          \
        _Pragma("GCC diagnostic pop")                                 \
    } while(0)

/* ── Magnitude dispatch — LMUL 1, 2, and 4 all supported ─────── */
#define MAGL1_RVV(img, gx, gy)                                        \
    do {                                                              \
        _Pragma("GCC diagnostic push")                                \
        _Pragma("GCC diagnostic ignored \"-Wunreachable-code\"")      \
        if constexpr (LMUL_SWEEP == 1)                                \
            (void)processing::MagL1<1>(img, gx, gy);                  \
        else if constexpr (LMUL_SWEEP == 2)                           \
            (void)processing::MagL1<2>(img, gx, gy);                  \
        else                                                          \
            (void)processing::MagL1<4>(img, gx, gy);                  \
        _Pragma("GCC diagnostic pop")                                 \
    } while(0)

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
    constexpr uint32_t WIDTH  = 100;
    constexpr uint32_t HEIGHT = 75;
    const size_t n = WIDTH * HEIGHT;

    image::io::metadata_t<uint8_t> image;
    image.width               = WIDTH;
    image.height              = HEIGHT;
    image.pixel_count         = n;
    image.aligned_buffer_size = n;
    image.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, n)));

    for (size_t i = 0; i < n; ++i)
        image.buffer[i] = static_cast<uint8_t>(i % 256);

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
        GAUSSIAN_RVV(blurred);
#else
        (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(blurred);
#endif

        /* Pre-compute gx/gy for magnitude stage */
#if PIPELINE_SEL == 3
#if defined(__riscv)
        SOBEL_RVV(blurred, gx, gy);
#else
        (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
#endif
#endif

        /* ── Stage 2: Sobel only ─────────────────────────────── */
#if (PIPELINE_SEL == 2) 
        {
            BENCH("Sobel Filter",
#if defined(__riscv)
                SOBEL_RVV(blurred, gx, gy)
#else
                (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy)
#endif
            ); // Added semi-colon outside macro boundary safely
        }
#endif

        /* ── Stage 3: Magnitude L1 only ──────────────────────── */
#if PIPELINE_SEL == 3
        {
            auto mag = make_copy(image);
            BENCH("Magnitude L1",
#if defined(__riscv)
                MAGL1_RVV(mag, gx, gy)
#else
                (void)processing::MagL1<2>(mag, gx, gy)
#endif
            );
        }
#endif
    }
#endif /* stages 2-3-6 */

    /* ── Stage 0: Gaussian spatial only ──────────────────────── */
#if PIPELINE_SEL == 0
    {
        auto img_copy = make_copy(image);
        BENCH("Gaussian spatial",
            std::copy_n(image.buffer.get(), n, img_copy.buffer.get());
#if defined(__riscv)
            GAUSSIAN_RVV(img_copy)
#else
            (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_copy)
#endif
        );
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
            GAUSSIAN_RVV(blurred);
            SOBEL_RVV(blurred, gx, gy);
            MAGL1_RVV(mag, gx, gy)
#else
            (void)processing::gaussian_spatial_5x5<uint8_t, int32_t>(blurred);
            (void)processing::sobel_3x3<uint8_t, int16_t>(blurred, gx, gy);
            (void)processing::MagL1<2>(mag, gx, gy)
#endif
        );
    }
#endif

    std::free(gx);
    std::free(gy);
    return 0;
}