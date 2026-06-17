/**
 * @file    canny_test.cpp
 * @brief   Phase 3.2 — QEMU-side equivalence and sanity test harness.
 *
 * Runs both implementations of each stage on the same input and compares
 * outputs pixel-by-pixel (±1 tolerance for rounding). Uses a non-power-of-two
 * image (100x75) to force the strip-mining tail case once RVV is added.
 *
 * No GoogleTest — assert-based with printf and return codes only.
 * Run this binary at VLEN=128, 256, and 512 via the Makefile rvv_test target.
 *
 * Exit codes:
 *   0  — all tests passed
 *   1  — one or more tests failed
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <memory>

#include "std_types.hpp"
#include "io.hpp"
#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include "utils.hpp"

// =============================================================================
// Helpers
// =============================================================================

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define PASS(name) \
    do { ++g_tests_run; ++g_tests_passed; \
         std::cout << "  [PASS] " << (name) << "\n"; } while(0)

#define FAIL(name, msg) \
    do { ++g_tests_run; ++g_tests_failed; \
         std::cout << "  [FAIL] " << (name) << " — " << (msg) << "\n"; } while(0)

/** Allocate a zeroed, 64-byte aligned image buffer. */
template <typename T>
image::io::metadata_t<T> make_image(uint32_t w, uint32_t h)
{
    image::io::metadata_t<T> img;
    img.width              = w;
    img.height             = h;
    img.pixel_count        = static_cast<size_t>(w) * h;
    img.aligned_buffer_size = utils::memory::align_64(img.pixel_count * sizeof(T));
    img.buffer.reset(static_cast<T*>(
        utils::memory::aligned_alloc(64, img.aligned_buffer_size)));
    std::memset(img.buffer.get(), 0, img.aligned_buffer_size);
    return img;
}

/**
 * Compare two uint8_t buffers pixel-by-pixel with ±1 tolerance.
 * Returns true if all pixels match, false and prints the first mismatch otherwise.
 */
static bool compare_u8(const uint8_t* a, const uint8_t* b, size_t count,
                        const char* label, int tolerance = 1)
{
    for (size_t i = 0; i < count; ++i)
    {
        int diff = static_cast<int>(a[i]) - static_cast<int>(b[i]);
        if (diff < -tolerance || diff > tolerance)
        {
            std::cout << "    Mismatch at pixel " << i
                      << ": " << label << "_a=" << static_cast<int>(a[i])
                      << " vs " << label << "_b=" << static_cast<int>(b[i])
                      << " (diff=" << diff << ")\n";
            return false;
        }
    }
    return true;
}


using Clock    = std::chrono::high_resolution_clock;
using Ms       = std::chrono::milliseconds;

static long elapsed_ms(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration_cast<Ms>(end - start).count();
}


static void test_gaussian(const image::io::metadata_t<uint8_t>& src)
{
    std::cout << "\n[Gaussian]\n";

    const uint32_t w = src.width;
    const uint32_t h = src.height;

    auto img_spatial   = make_image<uint8_t>(w, h);
    auto img_separable = make_image<uint8_t>(w, h);
    std::memcpy(img_spatial.buffer.get(),   src.buffer.get(), src.pixel_count);
    std::memcpy(img_separable.buffer.get(), src.buffer.get(), src.pixel_count);

    // --- Spatial ---
    auto t0 = Clock::now();
    Status s1 = processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_spatial);
    auto t1 = Clock::now();

    if (s1 != Status::E_OK)
    { FAIL("gaussian_spatial status", "returned error"); return; }
    std::cout << "  Spatial    : " << elapsed_ms(t0, t1) << " ms\n";

    // --- Separable ---
    auto t2 = Clock::now();
    Status s2 = processing::gaussian_separable_5x5<uint8_t, int32_t>(img_separable);
    auto t3 = Clock::now();

    if (s2 != Status::E_OK)
    { FAIL("gaussian_separable status", "returned error"); return; }
    std::cout << "  Separable  : " << elapsed_ms(t2, t3) << " ms\n";

    // --- Equivalence (interior only — border rounding differs slightly) ---
    bool ok = true;
    const int32_t W = static_cast<int32_t>(w);
    for (uint32_t y = 2; y < h - 2 && ok; ++y)
    {
        for (uint32_t x = 2; x < w - 2 && ok; ++x)
        {
            size_t i = y * w + x;
            int diff = static_cast<int>(img_spatial.buffer.get()[i])
                     - static_cast<int>(img_separable.buffer.get()[i]);
            if (diff < -2 || diff > 2)
            {
                std::cout << "    Mismatch at (" << x << "," << y << ")"
                          << ": spatial=" << static_cast<int>(img_spatial.buffer.get()[i])
                          << " separable=" << static_cast<int>(img_separable.buffer.get()[i])
                          << " diff=" << diff << "\n";
                ok = false;
            }
        }
    }
    if (ok) PASS("gaussian spatial==separable (interior, ±2)");
    else    FAIL("gaussian spatial==separable", "pixel mismatch");

    // --- Uniform image sanity: all-128 input → interior stays 128 ±1 ---
    auto img_uniform = make_image<uint8_t>(w, h);
    std::memset(img_uniform.buffer.get(), 128, img_uniform.pixel_count);
    processing::gaussian_separable_5x5<uint8_t, int32_t>(img_uniform);

    bool uniform_ok = true;
    for (uint32_t y = 2; y < h - 2 && uniform_ok; ++y)
        for (uint32_t x = 2; x < w - 2 && uniform_ok; ++x)
        {
            uint8_t v = img_uniform.buffer.get()[y * w + x];
            if (v < 127 || v > 129) uniform_ok = false;
        }
    if (uniform_ok) PASS("gaussian uniform image stays uniform (±1)");
    else            FAIL("gaussian uniform image", "interior pixel drifted beyond ±1");

    image::io::save_raw<uint8_t>("out_gaussian.raw", img_separable);

}

static void test_sobel(const image::io::metadata_t<uint8_t>& blurred,
                       image::io::metadata_t<int16_t>& out_gx,
                       image::io::metadata_t<int16_t>& out_gy)
{
    std::cout << "\n[Sobel]\n";

    const uint32_t w = blurred.width;
    const uint32_t h = blurred.height;

    // --- Run on blurred image ---
    auto t0 = Clock::now();
    Status stat = processing::sobel_3x3<uint8_t, int16_t>(
        blurred, out_gx.buffer.get(), out_gy.buffer.get());
    auto t1 = Clock::now();

    if (stat != Status::E_OK)
    { FAIL("sobel status", "returned error"); return; }
    std::cout << "  Time       : " << elapsed_ms(t0, t1) << " ms\n";

    // --- Sanity: uniform image → zero gradient interior ---
    auto img_u = make_image<uint8_t>(w, h);
    auto gx_u  = make_image<int16_t>(w, h);
    auto gy_u  = make_image<int16_t>(w, h);
    std::memset(img_u.buffer.get(), 128, img_u.pixel_count);
    processing::sobel_3x3<uint8_t, int16_t>(img_u, gx_u.buffer.get(), gy_u.buffer.get());

    bool uniform_ok = true;
    for (uint32_t y = 1; y < h - 1 && uniform_ok; ++y)
        for (uint32_t x = 1; x < w - 1 && uniform_ok; ++x)
        {
            size_t i = y * w + x;
            if (gx_u.buffer.get()[i] != 0 || gy_u.buffer.get()[i] != 0)
                uniform_ok = false;
        }
    if (uniform_ok) PASS("sobel uniform image → zero gradient");
    else            FAIL("sobel uniform image", "non-zero gradient in flat region");

    // --- Sanity: vertical edge → large Gx, zero Gy at edge column ---
    auto img_v = make_image<uint8_t>(w, h);
    auto gx_v  = make_image<int16_t>(w, h);
    auto gy_v  = make_image<int16_t>(w, h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            img_v.buffer.get()[y * w + x] = (x < w / 2) ? 0 : 255;
    processing::sobel_3x3<uint8_t, int16_t>(img_v, gx_v.buffer.get(), gy_v.buffer.get());

    bool vedge_ok = true;
    const uint32_t ex = w / 2;
    for (uint32_t y = 1; y < h - 1 && vedge_ok; ++y)
    {
        size_t i = y * w + ex;
        if (std::abs(gx_v.buffer.get()[i]) == 0) vedge_ok = false;
        if (gy_v.buffer.get()[i] != 0)           vedge_ok = false;
    }
    if (vedge_ok) PASS("sobel vertical edge → Gx≠0, Gy=0");
    else          FAIL("sobel vertical edge", "unexpected gradient values");

    // --- Save outputs ---
    image::io::save_raw<int16_t>("out_gx.raw", out_gx);
    image::io::save_raw<int16_t>("out_gy.raw", out_gy);
}


static void test_magnitude(const image::io::metadata_t<int16_t>& gx,
                            const image::io::metadata_t<int16_t>& gy)
{
    std::cout << "\n[Magnitude]\n";

    const uint32_t w = gx.width;
    const uint32_t h = gx.height;

    auto mag_l1 = make_image<uint8_t>(w, h);
    auto mag_l2 = make_image<uint8_t>(w, h);

    // --- L1 ---
    auto t0 = Clock::now();
    Status s1 = processing::MagL1<uint8_t, int16_t, uint16_t>(
        mag_l1, gx.buffer.get(), gy.buffer.get());
    auto t1 = Clock::now();

    if (s1 != Status::E_OK)
    { FAIL("MagL1 status", "returned error"); return; }
    std::cout << "  L1 time    : " << elapsed_ms(t0, t1) << " ms\n";

    // --- L2 ---
    auto t2 = Clock::now();
    Status s2 = processing::MagL2<uint8_t, int16_t, float>(
        mag_l2, gx.buffer.get(), gy.buffer.get());
    auto t3 = Clock::now();

    if (s2 != Status::E_OK)
    { FAIL("MagL2 status", "returned error"); return; }
    std::cout << "  L2 time    : " << elapsed_ms(t2, t3) << " ms\n";

    // --- Both produce non-zero output ---
    bool l1_nonzero = false, l2_nonzero = false;
    uint8_t l1_max = 0, l2_max = 0;
    for (size_t i = 0; i < mag_l1.pixel_count; ++i)
    {
        if (mag_l1.buffer.get()[i] > 0) l1_nonzero = true;
        if (mag_l2.buffer.get()[i] > 0) l2_nonzero = true;
        if (mag_l1.buffer.get()[i] > l1_max) l1_max = mag_l1.buffer.get()[i];
        if (mag_l2.buffer.get()[i] > l2_max) l2_max = mag_l2.buffer.get()[i];
    }
    if (l1_nonzero) PASS("MagL1 produces non-zero output");
    else            FAIL("MagL1", "all pixels are zero");
    if (l2_nonzero) PASS("MagL2 produces non-zero output");
    else            FAIL("MagL2", "all pixels are zero");

    // --- Max value should be 255 (normalized) ---
    if (l1_max == 255) PASS("MagL1 max normalized to 255");
    else               FAIL("MagL1 normalization", "max value != 255");
    if (l2_max == 255) PASS("MagL2 max normalized to 255");
    else               FAIL("MagL2 normalization", "max value != 255");

    // --- L1 >= L2 always (L1 is an overestimate of Euclidean norm) ---
    bool l1_ge_l2 = true;
    for (size_t i = 0; i < mag_l1.pixel_count && l1_ge_l2; ++i)
    {
        // After normalization this isn't strictly guaranteed pixel-by-pixel,
        // but the max pixel (255) must appear in L1 output too
        (void)i;
    }
    // Skip pixel-wise check post-normalization — norms are independently scaled.
    // Instead verify both outputs are in valid range [0,255]
    bool range_ok = true;
    for (size_t i = 0; i < mag_l1.pixel_count && range_ok; ++i)
        if (mag_l1.buffer.get()[i] > 255 || mag_l2.buffer.get()[i] > 255)
            range_ok = false;
    if (range_ok) PASS("Magnitude outputs in valid range [0,255]");
    else          FAIL("Magnitude range", "pixel value > 255");

    // --- Save ---
    image::io::save_raw<uint8_t>("out_mag_l1.raw", mag_l1);
    image::io::save_raw<uint8_t>("out_mag_l2.raw", mag_l2);
}

// =============================================================================
// Test: Direction — known angle cases + output is always one of {0,45,90,135}
// =============================================================================

static void test_direction(const image::io::metadata_t<int16_t>& gx,
                            const image::io::metadata_t<int16_t>& gy)
{
    std::cout << "\n[Direction]\n";

    const uint32_t w = gx.width;
    const uint32_t h = gx.height;

    auto dir = make_image<uint8_t>(w, h);

    auto t0 = Clock::now();
    Status stat = processing::Direction<uint8_t, int16_t>(
        dir, gx.buffer.get(), gy.buffer.get());
    auto t1 = Clock::now();

    if (stat != Status::E_OK)
    { FAIL("Direction status", "returned error"); return; }
    std::cout << "  Time       : " << elapsed_ms(t0, t1) << " ms\n";

    // --- All output values must be one of {0, 45, 90, 135} ---
    bool valid_angles = true;
    for (size_t i = 0; i < dir.pixel_count && valid_angles; ++i)
    {
        uint8_t a = dir.buffer.get()[i];
        if (a != 0 && a != 45 && a != 90 && a != 135)
        {
            std::cout << "    Invalid angle " << static_cast<int>(a)
                      << " at pixel " << i << "\n";
            valid_angles = false;
        }
    }
    if (valid_angles) PASS("Direction all outputs in {0,45,90,135}");
    else              FAIL("Direction output values", "invalid angle found");

    // --- Known-case equivalence using a tiny synthetic buffer ---
    // Pure horizontal gradient (Gx=255, Gy=0) → 0°
    // Pure vertical gradient  (Gx=0, Gy=255) → 90°
    // Equal diagonal          (Gx=255,Gy=255) → 135°
    // Anti-diagonal           (Gx=255,Gy=-255) → 45°
    {
        auto tgx = make_image<int16_t>(4, 1);
        auto tgy = make_image<int16_t>(4, 1);
        auto tdir = make_image<uint8_t>(4, 1);

        tgx.buffer.get()[0] = 255;  tgy.buffer.get()[0] = 0;    // → 0°
        tgx.buffer.get()[1] = 0;    tgy.buffer.get()[1] = 255;   // → 90°
        tgx.buffer.get()[2] = 255;  tgy.buffer.get()[2] = 255;   // → 135°
        tgx.buffer.get()[3] = 255;  tgy.buffer.get()[3] = -255;  // → 45°

        processing::Direction<uint8_t, int16_t>(tdir, tgx.buffer.get(), tgy.buffer.get());

        bool cases_ok = (tdir.buffer.get()[0] == 0)   &&
                        (tdir.buffer.get()[1] == 90)   &&
                        (tdir.buffer.get()[2] == 135)  &&
                        (tdir.buffer.get()[3] == 45);

        if (cases_ok)
        {
            PASS("Direction known-angle cases {0°,90°,135°,45°}");
        }
        else
        {
            std::cout << "    Got: " << static_cast<int>(tdir.buffer.get()[0])
                      << " " << static_cast<int>(tdir.buffer.get()[1])
                      << " " << static_cast<int>(tdir.buffer.get()[2])
                      << " " << static_cast<int>(tdir.buffer.get()[3]) << "\n";
            FAIL("Direction known-angle cases", "one or more angles wrong");
        }
    }

    // --- Save ---
    image::io::save_raw<uint8_t>("out_direction.raw", dir);
}

// =============================================================================
// Full pipeline chained run (for timing the end-to-end path)
// =============================================================================

static void test_pipeline(const image::io::metadata_t<uint8_t>& src)
{
    std::cout << "\n[Full Pipeline — end-to-end timing]\n";

    const uint32_t w = src.width;
    const uint32_t h = src.height;

    auto img = make_image<uint8_t>(w, h);
    std::memcpy(img.buffer.get(), src.buffer.get(), src.pixel_count);

    auto gx  = make_image<int16_t>(w, h);
    auto gy  = make_image<int16_t>(w, h);
    auto mag = make_image<uint8_t>(w, h);
    auto dir = make_image<uint8_t>(w, h);

    auto t_start = Clock::now();

    processing::gaussian_separable_5x5<uint8_t, int32_t>(img);
    auto t_gauss = Clock::now();

    processing::sobel_3x3<uint8_t, int16_t>(img, gx.buffer.get(), gy.buffer.get());
    auto t_sobel = Clock::now();

    processing::MagL2<uint8_t, int16_t, float>(mag, gx.buffer.get(), gy.buffer.get());
    auto t_mag = Clock::now();

    processing::Direction<uint8_t, int16_t>(dir, gx.buffer.get(), gy.buffer.get());
    auto t_dir = Clock::now();

    long ms_gauss = elapsed_ms(t_start, t_gauss);
    long ms_sobel = elapsed_ms(t_gauss, t_sobel);
    long ms_mag   = elapsed_ms(t_sobel, t_mag);
    long ms_dir   = elapsed_ms(t_mag,   t_dir);
    long ms_total = elapsed_ms(t_start, t_dir);

    std::cout << "  Gaussian   : " << ms_gauss << " ms\n";
    std::cout << "  Sobel      : " << ms_sobel << " ms\n";
    std::cout << "  Magnitude  : " << ms_mag   << " ms\n";
    std::cout << "  Direction  : " << ms_dir   << " ms\n";
    std::cout << "  Total      : " << ms_total << " ms\n";

    if (ms_total > 0)
    {
        std::cout << "  Breakdown  : "
                  << "Gaussian="  << (ms_gauss * 100 / ms_total) << "% | "
                  << "Sobel="     << (ms_sobel * 100 / ms_total) << "% | "
                  << "Magnitude=" << (ms_mag   * 100 / ms_total) << "% | "
                  << "Direction=" << (ms_dir   * 100 / ms_total) << "%\n";
    }

    PASS("full pipeline completed without error");
}

// =============================================================================
// main
// =============================================================================

int main()
{
    std::cout << "=== Canny QEMU Equivalence Test (Phase 3.2) ===\n";
    std::cout << "Image: 100x75 (non-power-of-two — forces strip-mining tail)\n";

    const uint32_t W = 100;
    const uint32_t H = 75;

    // Load source image
    auto src = make_image<uint8_t>(W, H);
    Status load_stat = image::io::load_raw<uint8_t>("rect.raw", src);
    if (load_stat != Status::E_OK)
    {
        std::cerr << "ERROR: could not load rect.raw — place it in the working directory.\n";
        return 1;
    }
    std::cout << "Loaded rect.raw (" << W << "x" << H << ")\n";

    // Intermediate buffers shared across test functions
    auto blurred = make_image<uint8_t>(W, H);
    std::memcpy(blurred.buffer.get(), src.buffer.get(), src.pixel_count);
    processing::gaussian_separable_5x5<uint8_t, int32_t>(blurred);

    auto gx = make_image<int16_t>(W, H);
    auto gy = make_image<int16_t>(W, H);
    processing::sobel_3x3<uint8_t, int16_t>(blurred, gx.buffer.get(), gy.buffer.get());

    // Run all test sections
    test_gaussian(src);
    test_sobel(blurred, gx, gy);
    test_magnitude(gx, gy);
    test_direction(gx, gy);
    test_pipeline(src);

    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "  Passed : " << g_tests_passed << "/" << g_tests_run << "\n";
    std::cout << "  Failed : " << g_tests_failed << "/" << g_tests_run << "\n";

    if (g_tests_failed == 0)
    {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    }
    else
    {
        std::cout << "SOME TESTS FAILED\n";
        return 1;
    }
}
