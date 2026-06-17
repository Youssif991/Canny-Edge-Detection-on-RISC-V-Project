#include "io.hpp"
#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include "std_types.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <time.h>
#include <sys/time.h>
#include <algorithm>



// ── Timing helper ─────────────────────────────────────────────
static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (e.tv_sec  - s.tv_sec)  * 1000.0
         + (e.tv_nsec - s.tv_nsec) / 1e6;
}

int main()
{
    constexpr uint32_t WIDTH  = 100;
    constexpr uint32_t HEIGHT = 75;
    constexpr int      ITERS  = 100;

    // ── 1. Load image ──────────────────────────────────────────
    image::io::metadata_t<uint8_t> image;
    image.width  = WIDTH;
    image.height = HEIGHT;
    image.pixel_count = static_cast<size_t>(WIDTH) * HEIGHT;
    image.aligned_buffer_size = utils::memory::align_64(image.pixel_count * sizeof(uint8_t));
    image.buffer.reset(static_cast<uint8_t*>(
        utils::memory::aligned_alloc(64, image.aligned_buffer_size)));

    // Just fill with a dummy value if we don't have tiger.raw around
     Status st = image::io::load_raw("rect.raw", image);
    // For benchmarking, memory content doesn't impact cycle count of these algorithms
    std::memset(image.buffer.get(), 128, image.aligned_buffer_size);

    // ── 2. Allocate Sobel output buffers ───────────────────────
    const size_t n = image.pixel_count;

    auto gx = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    auto gy = static_cast<int16_t*>(utils::memory::aligned_alloc(64, n * sizeof(int16_t)));
    if (!gx || !gy) { printf("ERROR: alloc failed\n"); return 1; }

    // ── 3. Benchmark helpers ───────────────────────────────────
    struct timespec t0, t1;
    double times[10] = {0};
    int bench_idx = 0;

#define BENCH(label, ...)                                       \
    clock_gettime(CLOCK_MONOTONIC, &t0);                        \
    for (int _i = 0; _i < ITERS; ++_i) { __VA_ARGS__; }         \
    clock_gettime(CLOCK_MONOTONIC, &t1);                        \
    times[bench_idx] = elapsed_ms(t0, t1) / ITERS;              \
    printf("%-20s %.3f ms/iter\n", label, times[bench_idx]);    \
    bench_idx++;

    // ── 4. Gaussian spatial ────────────────────────────────────
    {
        image::io::metadata_t<uint8_t> img_copy;
        img_copy.width              = image.width;
        img_copy.height             = image.height;
        img_copy.pixel_count        = image.pixel_count;
        img_copy.aligned_buffer_size = image.aligned_buffer_size;
        auto* raw = static_cast<uint8_t*>(
            utils::memory::aligned_alloc(64, image.aligned_buffer_size));
        img_copy.buffer.reset(raw);

        BENCH("Gaussian spatial",
            std::copy_n(image.buffer.get(), n, img_copy.buffer.get());
            processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_copy);
        )
    }

    // ── 5. Gaussian separable ──────────────────────────────────
    {
        image::io::metadata_t<uint8_t> img_copy;
        img_copy.width              = image.width;
        img_copy.height             = image.height;
        img_copy.pixel_count        = image.pixel_count;
        img_copy.aligned_buffer_size = image.aligned_buffer_size;
        auto* raw = static_cast<uint8_t*>(
            utils::memory::aligned_alloc(64, image.aligned_buffer_size));
        img_copy.buffer.reset(raw);

        BENCH("Gaussian separable",
            std::copy_n(image.buffer.get(), n, img_copy.buffer.get());
            processing::gaussian_separable_5x5<uint8_t, int32_t>(img_copy);
        )
    }

    // ── 6. Sobel ───────────────────────────────────────────────
    BENCH("Sobel",
        processing::sobel_3x3<uint8_t, int16_t>(image, gx, gy);
    )
    
    // ── 6b. Sobel Unbounded ────────────────────────────────────
    BENCH("Sobel Unbounded",
        processing::sobel_3x3_unbounded<uint8_t, int16_t>(image, gx, gy);
    )
    

    // ── 7. Magnitude L1 ───────────────────────────────────────
    {
        image::io::metadata_t<uint8_t> mag;
        mag.width              = image.width;
        mag.height             = image.height;
        mag.pixel_count        = image.pixel_count;
        mag.aligned_buffer_size = image.aligned_buffer_size;
        auto* raw = static_cast<uint8_t*>(
            utils::memory::aligned_alloc(64, image.aligned_buffer_size));
        mag.buffer.reset(raw);

        BENCH("Magnitude L1",
            processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx, gy);
        )
    }

    // ── 8. Magnitude L2 ───────────────────────────────────────
    {
        image::io::metadata_t<uint8_t> mag;
        mag.width              = image.width;
        mag.height             = image.height;
        mag.pixel_count        = image.pixel_count;
        mag.aligned_buffer_size = image.aligned_buffer_size;
        auto* raw = static_cast<uint8_t*>(
            utils::memory::aligned_alloc(64, image.aligned_buffer_size));
        mag.buffer.reset(raw);

        BENCH("Magnitude L2",
            processing::MagL2<uint8_t, int16_t, float>(mag, gx, gy);
        )
    }

    // ── 9. Direction ───────────────────────────────────────────
    {
        image::io::metadata_t<uint8_t> dir;
        dir.width              = image.width;
        dir.height             = image.height;
        dir.pixel_count        = image.pixel_count;
        dir.aligned_buffer_size = image.aligned_buffer_size;
        auto* raw = static_cast<uint8_t*>(
            utils::memory::aligned_alloc(64, image.aligned_buffer_size));
        dir.buffer.reset(raw);

        // My Direction code takes int16_t, no need to upcast!
        BENCH("Direction",
            processing::Direction<uint8_t, int16_t>(dir, gx, gy);
        )
    }

    std::free(gx);
    std::free(gy);

    printf("\nDone. Image: %ux%u, %d iterations each.\n", WIDTH, HEIGHT, ITERS);

    // Calculate percentage breakdown for standard pipeline:
    // times[1] = Gaussian separable
    // times[2] = Sobel (Standard)
    // times[4] = Magnitude L1
    // times[6] = Direction
    if (bench_idx >= 7) {
        double total_time = times[1] + times[2] + times[4] + times[6];
        printf("\n--- Pipeline Profiling Breakdown ---\n");
        printf("Gaussian: %.1f%% | Sobel Gx/Gy: %.1f%% | Magnitude: %.1f%% | Direction: %.1f%%\n",
               (times[1] / total_time) * 100.0,
               (times[2] / total_time) * 100.0,
               (times[4] / total_time) * 100.0,
               (times[6] / total_time) * 100.0);
    }

    return 0;
}
