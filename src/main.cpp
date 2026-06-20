/**
 * @file    main.cpp
 * @brief   Canny Edge Detection — full pipeline.
 *
 * Loads a raw grayscale image, runs every stage in order, and saves
 * each intermediate result as a .raw file in ./assets/.
 *
 * Change IMAGE_FILE, IMAGE_WIDTH, IMAGE_HEIGHT to switch input.
 * RVV paths are compiled in automatically when -march=rv64gcv is used.
 */

#include "std_types.hpp"
#include "io.hpp"
#include "gaussian.hpp"
#include "gaussian_rvv.hpp"
#include "sobel.hpp"
#include "sobel_rvv.hpp"
#include "magnitude.hpp"
#include "magnitude_rvv.hpp"
#include "direction.hpp"
#include "nms.hpp"
#include "double_threshold.hpp"
#include "hysteresis.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

// ── Configure input here ─────────────────────────────────────────────────────
#define IMAGE_FILE   "Ben10.raw"
#define IMAGE_WIDTH  1920
#define IMAGE_HEIGHT 1080
// ─────────────────────────────────────────────────────────────────────────────

/** Allocate a zeroed 64-byte aligned image buffer. */
template <typename T>
static image::io::metadata_t<T> make_image(uint32_t w, uint32_t h)
{
    image::io::metadata_t<T> img;
    img.width               = w;
    img.height              = h;
    img.pixel_count         = static_cast<size_t>(w) * h;
    img.aligned_buffer_size = utils::memory::align_64(img.pixel_count * sizeof(T));
    img.buffer.reset(static_cast<T *>(
        utils::memory::aligned_alloc(64, img.aligned_buffer_size)));
    std::memset(img.buffer.get(), 0, img.aligned_buffer_size);
    return img;
}

/** Check status and abort with a message if it failed. */
static void check(Status s, const char *stage)
{
    if (s != Status::E_OK)
    {
        std::printf("ERROR: %s failed (status=%d)\n", stage, static_cast<int>(s));
        std::exit(1);
    }
}

int main()
{
    constexpr uint32_t W = IMAGE_WIDTH;
    constexpr uint32_t H = IMAGE_HEIGHT;

    std::printf("=== Canny Edge Detection Pipeline ===\n");
    std::printf("Image : %s  (%ux%u)\n", IMAGE_FILE, W, H);
#ifdef __riscv_vector
    std::printf("Mode  : RVV  (Gaussian=LMUL2  Sobel=LMUL2  Magnitude=LMUL4)\n\n");
#else
    std::printf("Mode  : Scalar\n\n");
#endif

    // ── 1. Load ──────────────────────────────────────────────────────────────
    auto src = make_image<uint8_t>(W, H);
    check(image::io::load_raw<uint8_t>(IMAGE_FILE, src), "load_raw");
    std::printf("[1] Loaded %s\n", IMAGE_FILE);

    // ── 2. Gaussian blur ─────────────────────────────────────────────────────
    auto blurred = make_image<uint8_t>(W, H);
    std::memcpy(blurred.buffer.get(), src.buffer.get(), src.pixel_count);

#ifdef __riscv_vector
    check(processing::gaussian_spatial_5x5_rvv(blurred), "gaussian_rvv");
    std::printf("[2] Gaussian blur      (RVV LMUL=2)\n");
#else
    check(processing::gaussian_spatial_5x5<uint8_t, int32_t>(blurred), "gaussian_scalar");
    std::printf("[2] Gaussian blur      (scalar)\n");
#endif

    check(image::io::save_raw<uint8_t>("out_gaussian.raw", blurred), "save gaussian");

    // ── 3. Sobel gradients ───────────────────────────────────────────────────
    auto gx = make_image<int16_t>(W, H);
    auto gy = make_image<int16_t>(W, H);

#ifdef __riscv_vector
    check(processing::sobel_3x3_rvv<2>(blurred, gx.buffer.get(), gy.buffer.get()), "sobel_rvv");
    std::printf("[3] Sobel gradients    (RVV LMUL=2)\n");
#else
    check(processing::sobel_3x3<uint8_t, int16_t>(blurred, gx.buffer.get(), gy.buffer.get()), "sobel_scalar");
    std::printf("[3] Sobel gradients    (scalar)\n");
#endif

    check(image::io::save_raw<int16_t>("out_sobel_gx.raw", gx), "save sobel gx");
    check(image::io::save_raw<int16_t>("out_sobel_gy.raw", gy), "save sobel gy");

    // ── 4. Gradient magnitude ────────────────────────────────────────────────
    auto mag = make_image<uint8_t>(W, H);

#ifdef __riscv_vector
    check(processing::MagL1<4>(mag, gx.buffer.get(), gy.buffer.get()), "magnitude_rvv");
    std::printf("[4] Gradient magnitude (RVV LMUL=4)\n");
#else
    check(processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx.buffer.get(), gy.buffer.get()), "magnitude_scalar");
    std::printf("[4] Gradient magnitude (scalar)\n");
#endif

    check(image::io::save_raw<uint8_t>("out_magnitude.raw", mag), "save magnitude");

    // ── 5. Gradient direction ────────────────────────────────────────────────
    auto dir = make_image<uint8_t>(W, H);
    check(processing::Direction<uint8_t, int16_t>(dir, gx.buffer.get(), gy.buffer.get()), "direction");
    std::printf("[5] Gradient direction (scalar)\n");

    check(image::io::save_raw<uint8_t>("out_direction.raw", dir), "save direction");

    // ── 6. Non-Maximum Suppression ───────────────────────────────────────
    auto nms_image = make_image<uint8_t>(W, H);
    check(processing::NonMaxSuppression<uint8_t>(mag, dir, nms_image), "nms");
    std::printf("[6] Non-Max Suppress   (scalar)\n");

    check(image::io::save_raw<uint8_t>("out_nms.raw", nms_image), "save nms");

    // ── 7. Double Threshold ──────────────────────────────────────────────
    // Using Auto-Otsu thresholding with a 0.4f multiplier as defined in bonus_test.cpp
    check(processing::DoubleThresholdAuto<uint8_t>(nms_image, 0.4f), "double_threshold");
    std::printf("[7] Double Threshold   (scalar)\n");

    check(image::io::save_raw<uint8_t>("out_double_threshold.raw", nms_image), "save double threshold");

    // ── 8. Hysteresis ────────────────────────────────────────────────────
    check(processing::Hysteresis<uint8_t>(nms_image), "hysteresis");
    std::printf("[8] Hysteresis Tracking(scalar)\n");

    check(image::io::save_raw<uint8_t>("out_hysteresis.raw", nms_image), "save hysteresis");

    // ── Done ─────────────────────────────────────────────────────────────────
    std::printf("\nDone. Outputs written to ./assets/\n");
    std::printf("  out_gaussian.raw\n");
    std::printf("  out_sobel_gx.raw\n");
    std::printf("  out_sobel_gy.raw\n");
    std::printf("  out_magnitude.raw\n");
    std::printf("  out_direction.raw\n");
    std::printf("  out_nms.raw\n");
    std::printf("  out_double_threshold.raw\n");
    std::printf("  out_hysteresis.raw\n");

    return 0;
}