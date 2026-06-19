#include "gaussian.hpp"
#include "gaussian_rvv.hpp"
#include "sobel.hpp"
#include "sobel_rvv.hpp"
#include "magnitude.hpp"
#include "magnitude_rvv.hpp"
#include "direction.hpp"
#include "std_types.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <chrono>

/* ── CHANGE THESE TO CONFIGURE THE RUN ──────────────────────────
 *
 *  PIPELINE_SEL : which stage to benchmark
 *      0 → Gaussian spatial only
 *      2 → Sobel only            (uses pre-blurred input)
 *      3 → Magnitude L1 only     (uses pre-computed gx/gy)
 *      4 → Direction only        (uses pre-computed gx/gy)
 *      6 → Full pipeline         (all stages timed individually + total)
 *
 *  RVV defaults (fixed):
 *      Gaussian  → LMUL 2
 *      Sobel     → LMUL 2
 *      Magnitude → LMUL 4
 *      Direction → scalar (not yet vectorized)
 *
 *  Scalar vs RVV is controlled by the Makefile -march flag:
 *      bench-scalar-O3  →  -march=rv64gc   (__riscv undefined)
 *      bench-rvv-O3     →  -march=rv64gcv  (__riscv defined)
 *
 * ────────────────────────────────────────────────────────────── */
#define PIPELINE_SEL 6

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
        printf("%-24s %.3f ms\n", label, elapsed_ms(t0, t1));    \
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

/* ── Shared pre-computation helpers ──────────────────────────── */

// Blur the image in-place (RVV or scalar depending on -march)
static void do_gaussian(image::io::metadata_t<uint8_t>& img)
{
#if defined(__riscv)
    (void)processing::gaussian_spatial_5x5_rvv_lmul2(img);
#endif
}

// Compute Gx/Gy from a blurred image
static void do_sobel(const image::io::metadata_t<uint8_t>& img,
                     int16_t* gx, int16_t* gy)
{
#if defined(__riscv)
    (void)processing::sobel_3x3_rvv<2>(img, gx, gy);
#endif
}

// Compute magnitude from Gx/Gy
static void do_magnitude(image::io::metadata_t<uint8_t>& mag,
                         const int16_t* gx, const int16_t* gy)
{
#if defined(__riscv)
    (void)processing::MagL1<4>(mag, gx, gy);
#endif
}

// Compute direction from Gx/Gy (scalar only — not yet vectorized)
static void do_direction(image::io::metadata_t<uint8_t>& dir,
                         const int16_t* gx, const int16_t* gy)
{
    (void)processing::Direction<uint8_t, int16_t>(dir, gx, gy);
}

/* ═══════════════════════════════════════════════════════════════ */

int main()
{
    constexpr uint32_t WIDTH  = 512;
    constexpr uint32_t HEIGHT = 512;
    const size_t n = WIDTH * HEIGHT;

    image::io::metadata_t<uint8_t> image;
    image.width               = WIDTH;
    image.height              = HEIGHT;
    image.pixel_count         = n;
    image.aligned_buffer_size = n;
    image.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, n)));

    for (size_t i = 0; i < n; ++i)
        image.buffer[i] = static_cast<uint8_t>(i % 256);

    /* ── Allocate gradient buffers ───────────────────────────── */
    auto* gx = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    auto* gy = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    if (!gx || !gy) { printf("ERROR: alloc failed\n"); return 1; }

    printf("--- Benchmark (%s) ---\n",
#if defined(__riscv)
           "RVV: Gaussian=LMUL2  Sobel=LMUL2  Magnitude=LMUL4  Direction=scalar"
#else
           "Scalar"
#endif
    );

    /* ── Stage 0: Gaussian only ───────────────────────────────── */
#if PIPELINE_SEL == 0
    {
        auto blurred = make_copy(image);
        BENCH("Gaussian spatial",
            std::copy_n(image.buffer.get(), n, blurred.buffer.get());
            do_gaussian(blurred);
        )
    }
#endif

    /* ── Stage 2: Sobel only ─────────────────────────────────── */
#if PIPELINE_SEL == 2
    {
        auto blurred = make_copy(image);
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        do_gaussian(blurred);

        BENCH("Sobel Filter",
            do_sobel(blurred, gx, gy);
        )
    }
#endif

    /* ── Stage 3: Magnitude only ─────────────────────────────── */
#if PIPELINE_SEL == 3
    {
        auto blurred = make_copy(image);
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        do_gaussian(blurred);
        do_sobel(blurred, gx, gy);

        auto mag = make_copy(image);
        BENCH("Magnitude L1",
            do_magnitude(mag, gx, gy);
        )
    }
#endif

    /* ── Stage 4: Direction only ─────────────────────────────── */
#if PIPELINE_SEL == 4
    {
        auto blurred = make_copy(image);
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        do_gaussian(blurred);
        do_sobel(blurred, gx, gy);

        auto dir = make_copy(image);
        BENCH("Direction",
            do_direction(dir, gx, gy);
        )
    }
#endif

    /* ── Stage 6: Full pipeline ───────────────────────────────── */
#if PIPELINE_SEL == 6
{
    auto blurred = make_copy(image);
    auto mag     = make_copy(image);
    auto dir     = make_copy(image);

    BENCH("Gaussian spatial",
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        do_gaussian(blurred);
    )

    // Re-copy fresh unblurred data before Sobel so it doesn't
    // benefit from Gaussian's hot cache
    {
        auto fresh = make_copy(image);
        std::copy_n(image.buffer.get(), n, fresh.buffer.get());
        do_gaussian(fresh);  // re-blur into fresh
        BENCH("Sobel Filter",
            do_sobel(fresh, gx, gy);
        )
    }

    {
        auto fresh = make_copy(image);
        std::copy_n(image.buffer.get(), n, fresh.buffer.get());
        do_gaussian(fresh);
        do_sobel(fresh, gx, gy);
        BENCH("Magnitude L1",
            do_magnitude(mag, gx, gy);
        )
    }

    {
        auto fresh = make_copy(image);
        std::copy_n(image.buffer.get(), n, fresh.buffer.get());
        do_gaussian(fresh);
        do_sobel(fresh, gx, gy);
        BENCH("Direction",
            do_direction(dir, gx, gy);
        )
    }

    printf("────────────────────────────────\n");

    BENCH("Total pipeline",
        std::copy_n(image.buffer.get(), n, blurred.buffer.get());
        do_gaussian(blurred);
        do_sobel(blurred, gx, gy);
        do_magnitude(mag, gx, gy);
        do_direction(dir, gx, gy);
    )
}
#endif

    std::free(gx);
    std::free(gy);
    return 0;
}