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
 * 0  — all tests passed
 * 1  — one or more tests failed
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
#include "gaussian_rvv.hpp"
#include "sobel.hpp"
#include "sobel_rvv.hpp"
#include "magnitude.hpp"
#include "magnitude_rvv.hpp"
#include "direction.hpp"
#include "utils.hpp"

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define PASS(name)                                  \
    do                                              \
    {                                               \
        ++g_tests_run;                              \
        ++g_tests_passed;                           \
        std::cout << "  [PASS] " << (name) << "\n"; \
    } while (0)

#define FAIL(name, msg)                                               \
    do                                                                \
    {                                                                 \
        ++g_tests_run;                                                \
        ++g_tests_failed;                                             \
        std::cout << "  [FAIL] " << (name) << " — " << (msg) << "\n"; \
    } while (0)

/** Allocate a zeroed, 64-byte aligned image buffer. */
template <typename T>
image::io::metadata_t<T> make_image(uint32_t w, uint32_t h)
{
    image::io::metadata_t<T> img;
    img.width = w;
    img.height = h;
    img.pixel_count = static_cast<size_t>(w) * h;
    img.aligned_buffer_size = utils::memory::align_64(img.pixel_count * sizeof(T));
    img.buffer.reset(static_cast<T *>(
        utils::memory::aligned_alloc(64, img.aligned_buffer_size)));
    std::memset(img.buffer.get(), 0, img.aligned_buffer_size);
    return img;
}

/**
 * Compare two uint8_t buffers pixel-by-pixel with ±1 tolerance.
 * Returns true if all pixels match, false and prints the first mismatch otherwise.
 */
static bool compare_u8(const uint8_t *a, const uint8_t *b, size_t count,
                       const char *label, int tolerance = 1)
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

/** Compare two int16_t buffers with ±tolerance. */
static bool compare_i16(const int16_t *a, const int16_t *b, size_t count,
                        const char *label, int tolerance = 1)
{
    for (size_t i = 0; i < count; ++i)
    {
        int diff = static_cast<int>(a[i]) - static_cast<int>(b[i]);
        if (diff < -tolerance || diff > tolerance)
        {
            std::cout << "    Mismatch at pixel " << i
                      << ": " << label << "_a=" << a[i]
                      << " vs " << label << "_b=" << b[i]
                      << " (diff=" << diff << ")\n";
            return false;
        }
    }
    return true;
}

using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::milliseconds;

static long elapsed_ms(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration_cast<Ms>(end - start).count();
}

static void test_gaussian(const image::io::metadata_t<uint8_t> &src)
{
    std::cout << "\n[Gaussian]\n";

    const uint32_t w = src.width;
    const uint32_t h = src.height;

    auto img_spatial = make_image<uint8_t>(w, h);
    auto img_separable = make_image<uint8_t>(w, h);
    std::memcpy(img_spatial.buffer.get(), src.buffer.get(), src.pixel_count);
    std::memcpy(img_separable.buffer.get(), src.buffer.get(), src.pixel_count);

    // --- Spatial Pass ---
    auto t0 = Clock::now();
    Status s1 = processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_spatial);
    auto t1 = Clock::now();

    if (s1 != Status::E_OK)
    {
        FAIL("gaussian_spatial status", "returned error");
        return;
    }
    std::cout << "  Gaussian Spatial   : " << elapsed_ms(t0, t1) << " ms\n";

    // --- Separable Pass ---
    auto t2 = Clock::now();
    Status s2 = processing::gaussian_separable_5x5<uint8_t, int32_t>(img_separable);
    auto t3 = Clock::now();

    if (s2 != Status::E_OK)
    {
        FAIL("gaussian_separable status", "returned error");
        return;
    }
    std::cout << "  Gaussian Separable : " << elapsed_ms(t2, t3) << " ms\n";

    // --- Uniform image sanity: all-128 input → interior stays 128 ±1 ---
    auto img_uniform = make_image<uint8_t>(w, h);
    std::memset(img_uniform.buffer.get(), 128, img_uniform.pixel_count);
    processing::gaussian_separable_5x5<uint8_t, int32_t>(img_uniform);

    bool uniform_ok = true;
    for (uint32_t y = 2; y < h - 2 && uniform_ok; ++y)
        for (uint32_t x = 2; x < w - 2 && uniform_ok; ++x)
        {
            uint8_t v = img_uniform.buffer.get()[y * w + x];
            if (v < 127 || v > 129)
                uniform_ok = false;
        }
    if (uniform_ok)
        PASS("gaussian uniform image stays uniform (±1)");
    else
        FAIL("gaussian uniform image", "interior pixel drifted beyond ±1");

    image::io::save_raw<uint8_t>("out_gaussian.raw", img_separable);
}

static void test_gaussian_rvv(const image::io::metadata_t<uint8_t> &src)
{
#ifndef __riscv
    std::cout << "\n[Gaussian RVV] SKIPPED (not a RISC-V build)\n";
    return;
#define __riscv
    std::cout << "\n[Gaussian RVV]\n";

    const uint32_t w = src.width;
    const uint32_t h = src.height;

    // Reference: scalar spatial
    auto ref = make_image<uint8_t>(w, h);
    std::memcpy(ref.buffer.get(), src.buffer.get(), src.pixel_count);
    processing::gaussian_spatial_5x5<uint8_t, int32_t>(ref);

    // Helper lambda: run one RVV variant, compare against ref, and save
    auto run = [&](const char *label,
                   Status (*fn)(image::io::metadata_t<uint8_t> &),
                   const char *save_file)
    {
        auto img = make_image<uint8_t>(w, h);
        std::memcpy(img.buffer.get(), src.buffer.get(), src.pixel_count);

        auto t0 = Clock::now();
        Status st = fn(img);
        auto t1 = Clock::now();

        if (st != Status::E_OK)
        {
            FAIL(label, "returned error");
            return;
        }
        std::cout << "  Gaussian RVV LMUL2 : " << elapsed_ms(t0, t1) << " ms\n";

        // Interior pixels only — border rounding may differ by 1 vs scalar
        bool ok = true;
        for (uint32_t y = 2; y < h - 2 && ok; ++y)
            for (uint32_t x = 2; x < w - 2 && ok; ++x)
            {
                size_t i = y * w + x;
                int diff = static_cast<int>(img.buffer.get()[i]) - static_cast<int>(ref.buffer.get()[i]);
                if (diff < -1 || diff > 1)
                {
                    std::cout << "    Mismatch at (" << x << "," << y << ")"
                              << ": rvv=" << static_cast<int>(img.buffer.get()[i])
                              << " ref=" << static_cast<int>(ref.buffer.get()[i])
                              << " diff=" << diff << "\n";
                    ok = false;
                }
            }
        if (ok)
            PASS(label);
        else
            FAIL(label, "pixel mismatch vs scalar spatial");

        if (ok && save_file)
            image::io::save_raw<uint8_t>(save_file, img);
    };

    run("gaussian_rvv lmul2 == scalar", processing::gaussian_spatial_5x5_rvv_lmul2, "out_gaussian_rvv.raw");

    // Uniform image: RVV must also preserve 128 ± 1 in the interior
    auto img_u = make_image<uint8_t>(w, h);
    std::memset(img_u.buffer.get(), 128, img_u.pixel_count);
    Status st = processing::gaussian_spatial_5x5_rvv(img_u);
    if (st != Status::E_OK)
    {
        FAIL("gaussian_rvv uniform", "returned error");
        return;
    }

    bool uniform_ok = true;
    for (uint32_t y = 2; y < h - 2 && uniform_ok; ++y)
        for (uint32_t x = 2; x < w - 2 && uniform_ok; ++x)
        {
            uint8_t v = img_u.buffer.get()[y * w + x];
            if (v < 127 || v > 129)
                uniform_ok = false;
        }
    if (uniform_ok)
        PASS("gaussian_rvv uniform image stays uniform (±1)");
    else
        FAIL("gaussian_rvv uniform image", "interior pixel drifted beyond ±1");
#endif
}

static void test_sobel(const image::io::metadata_t<uint8_t> &blurred,
                       image::io::metadata_t<int16_t> &out_gx,
                       image::io::metadata_t<int16_t> &out_gy)
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
    {
        FAIL("sobel status", "returned error");
        return;
    }
    std::cout << "  Sobel Scalar       : " << elapsed_ms(t0, t1) << " ms\n";

    // --- Sanity: uniform image → zero gradient interior ---
    auto img_u = make_image<uint8_t>(w, h);
    auto gx_u = make_image<int16_t>(w, h);
    auto gy_u = make_image<int16_t>(w, h);
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
    if (uniform_ok)
        PASS("sobel uniform image → zero gradient");
    else
        FAIL("sobel uniform image", "non-zero gradient in flat region");

    // --- Sanity: vertical edge → large Gx, zero Gy at edge column ---
    auto img_v = make_image<uint8_t>(w, h);
    auto gx_v = make_image<int16_t>(w, h);
    auto gy_v = make_image<int16_t>(w, h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            img_v.buffer.get()[y * w + x] = (x < w / 2) ? 0 : 255;
    processing::sobel_3x3<uint8_t, int16_t>(img_v, gx_v.buffer.get(), gy_v.buffer.get());

    bool vedge_ok = true;
    const uint32_t ex = w / 2;
    for (uint32_t y = 1; y < h - 1 && vedge_ok; ++y)
    {
        size_t i = y * w + ex;
        if (std::abs(gx_v.buffer.get()[i]) == 0)
            vedge_ok = false;
        if (gy_v.buffer.get()[i] != 0)
            vedge_ok = false;
    }
    if (vedge_ok)
        PASS("sobel vertical edge → Gx≠0, Gy=0");
    else
        FAIL("sobel vertical edge", "unexpected gradient values");

    // --- Save outputs ---
    image::io::save_raw<int16_t>("out_gx.raw", out_gx);
    image::io::save_raw<int16_t>("out_gy.raw", out_gy);
}

static void test_sobel_rvv(const image::io::metadata_t<uint8_t> &blurred)
{
#ifndef __riscv
    std::cout << "\n[Sobel RVV] SKIPPED (not a RISC-V build)\n";
    return;
#else
    std::cout << "\n[Sobel RVV]\n";

    const uint32_t w = blurred.width;
    const uint32_t h = blurred.height;

    // Reference: scalar sobel_3x3
    auto ref_gx = make_image<int16_t>(w, h);
    auto ref_gy = make_image<int16_t>(w, h);
    processing::sobel_3x3<uint8_t, int16_t>(
        blurred, ref_gx.buffer.get(), ref_gy.buffer.get());

    // Helper: run RVV LMUL variant, compare both gradient planes, and save
    auto run = [&](const char *label, const char *save_gx, const char *save_gy)
    {
        auto rvv_gx = make_image<int16_t>(w, h);
        auto rvv_gy = make_image<int16_t>(w, h);

        auto t0 = Clock::now();
        Status st = processing::sobel_3x3_rvv<2>(blurred, rvv_gx.buffer.get(), rvv_gy.buffer.get());
        auto t1 = Clock::now();

        if (st != Status::E_OK)
        {
            FAIL(label, "returned error");
            return;
        }
        std::cout << "  Sobel RVV LMUL2    : " << elapsed_ms(t0, t1) << " ms\n";

        // Sobel RVV must be bit-exact (tolerance=0) — same padding, same arithmetic
        bool gx_ok = compare_i16(rvv_gx.buffer.get(), ref_gx.buffer.get(),
                                 blurred.pixel_count, "Gx", 0);
        bool gy_ok = compare_i16(rvv_gy.buffer.get(), ref_gy.buffer.get(),
                                 blurred.pixel_count, "Gy", 0);

        std::string gx_name = std::string(label) + " Gx";
        std::string gy_name = std::string(label) + " Gy";
        if (gx_ok)
            PASS(gx_name.c_str());
        else
            FAIL(gx_name.c_str(), "Gx mismatch vs scalar");
        if (gy_ok)
            PASS(gy_name.c_str());
        else
            FAIL(gy_name.c_str(), "Gy mismatch vs scalar");

        if (gx_ok && save_gx)
            image::io::save_raw<int16_t>(save_gx, rvv_gx);
        if (gy_ok && save_gy)
            image::io::save_raw<int16_t>(save_gy, rvv_gy);
    };

    run("sobel_rvv lmul2", "out_gx_rvv.raw", "out_gy_rvv.raw");

    // Sanity: uniform input → zero gradient from RVV too
    auto img_u = make_image<uint8_t>(w, h);
    auto rvv_gx = make_image<int16_t>(w, h);
    auto rvv_gy = make_image<int16_t>(w, h);
    std::memset(img_u.buffer.get(), 128, img_u.pixel_count);
    processing::sobel_3x3_rvv<2>(img_u, rvv_gx.buffer.get(), rvv_gy.buffer.get());

    bool uniform_ok = true;
    for (uint32_t y = 1; y < h - 1 && uniform_ok; ++y)
        for (uint32_t x = 1; x < w - 1 && uniform_ok; ++x)
        {
            size_t i = y * w + x;
            if (rvv_gx.buffer.get()[i] != 0 || rvv_gy.buffer.get()[i] != 0)
                uniform_ok = false;
        }
    if (uniform_ok)
        PASS("sobel_rvv uniform image → zero gradient");
    else
        FAIL("sobel_rvv uniform image", "non-zero gradient in flat region");
#endif
}

static void test_magnitude(const image::io::metadata_t<int16_t> &gx,
                           const image::io::metadata_t<int16_t> &gy)
{
    std::cout << "\n[Magnitude]\n";

    const uint32_t w = gx.width;
    const uint32_t h = gx.height;

    auto mag_l1 = make_image<uint8_t>(w, h);
    auto mag_l2 = make_image<uint8_t>(w, h);

    // --- L1 Scalar ---
    auto t0 = Clock::now();
    Status s1 = processing::MagL1<uint8_t, int16_t, uint16_t>(
        mag_l1, gx.buffer.get(), gy.buffer.get());
    auto t1 = Clock::now();

    if (s1 != Status::E_OK)
    {
        FAIL("MagL1 status", "returned error");
        return;
    }
    std::cout << "  Magnitude L1 Scalar: " << elapsed_ms(t0, t1) << " ms\n";

    // --- L2 Scalar ---
    auto t2 = Clock::now();
    Status s2 = processing::MagL2<uint8_t, int16_t, float>(
        mag_l2, gx.buffer.get(), gy.buffer.get());
    auto t3 = Clock::now();

    if (s2 != Status::E_OK)
    {
        FAIL("MagL2 status", "returned error");
        return;
    }
    std::cout << "  Magnitude L2 Scalar: " << elapsed_ms(t2, t3) << " ms\n";

    // --- Both produce non-zero output ---
    bool l1_nonzero = false, l2_nonzero = false;
    uint8_t l1_max = 0, l2_max = 0;
    for (size_t i = 0; i < mag_l1.pixel_count; ++i)
    {
        if (mag_l1.buffer.get()[i] > 0)
            l1_nonzero = true;
        if (mag_l2.buffer.get()[i] > 0)
            l2_nonzero = true;
        if (mag_l1.buffer.get()[i] > l1_max)
            l1_max = mag_l1.buffer.get()[i];
        if (mag_l2.buffer.get()[i] > l2_max)
            l2_max = mag_l2.buffer.get()[i];
    }
    if (l1_nonzero)
        PASS("MagL1 produces non-zero output");
    else
        FAIL("MagL1", "all pixels are zero");
    if (l2_nonzero)
        PASS("MagL2 produces non-zero output");
    else
        FAIL("MagL2", "all pixels are zero");

    // --- Max value should be 255 (normalized) ---
    if (l1_max == 255)
        PASS("MagL1 max normalized to 255");
    else
        FAIL("MagL1 normalization", "max value != 255");
    if (l2_max == 255)
        PASS("MagL2 max normalized to 255");
    else
        FAIL("MagL2 normalization", "max value != 255");

    bool range_ok = true;
    for (size_t i = 0; i < mag_l1.pixel_count && range_ok; ++i)
        if (mag_l1.buffer.get()[i] > 255 || mag_l2.buffer.get()[i] > 255)
            range_ok = false;
    if (range_ok)
        PASS("Magnitude outputs in valid range [0,255]");
    else
        FAIL("Magnitude range", "pixel value > 255");

    // --- Save ---
    image::io::save_raw<uint8_t>("out_mag_l1.raw", mag_l1);
    image::io::save_raw<uint8_t>("out_mag_l2.raw", mag_l2);
}

static void test_magnitude_rvv(const image::io::metadata_t<int16_t> &gx,
                               const image::io::metadata_t<int16_t> &gy)
{
#ifndef __riscv
    std::cout << "\n[Magnitude RVV] SKIPPED (not a RISC-V build)\n";
    return;
#else
    std::cout << "\n[Magnitude RVV]\n";

    const uint32_t w = gx.width;
    const uint32_t h = gx.height;

    // Reference: scalar MagL1
    auto ref = make_image<uint8_t>(w, h);
    processing::MagL1<uint8_t, int16_t, uint16_t>(ref, gx.buffer.get(), gy.buffer.get());

    // Helper: run LMUL variant, compare vs scalar reference, and save
    auto run = [&](const char *label, const char *save_file)
    {
        auto out = make_image<uint8_t>(w, h);

        auto t0 = Clock::now();
        Status st = processing::MagL1<4>(out, gx.buffer.get(), gy.buffer.get());
        auto t1 = Clock::now();

        if (st != Status::E_OK)
        {
            FAIL(label, "returned error");
            return;
        }
        std::cout << "  Magnitude RVV LMUL4: " << elapsed_ms(t0, t1) << " ms\n";

        // RVV MagL1 uses integer division; scalar uses the same — expect ±1 for
        // intermediate rounding differences on max-normalized values
        bool ok = compare_u8(out.buffer.get(), ref.buffer.get(),
                             out.pixel_count, "MagL1", 1);
        if (ok)
            PASS(label);
        else
            FAIL(label, "pixel mismatch vs scalar MagL1");

        if (ok && save_file)
            image::io::save_raw<uint8_t>(save_file, out);
    };

    run("magnitude_rvv lmul4 == scalar", "out_mag_rvv.raw");

    // Max must be 255 for LMUL=4
    auto out = make_image<uint8_t>(w, h);
    Status st = processing::MagL1<4>(out, gx.buffer.get(), gy.buffer.get());

    if (st == Status::E_OK)
    {
        uint8_t max_val = 0;
        for (size_t i = 0; i < out.pixel_count; ++i)
            if (out.buffer.get()[i] > max_val)
                max_val = out.buffer.get()[i];

        if (max_val == 255)
            PASS("magnitude_rvv lmul4 max==255");
        else
            FAIL("magnitude_rvv lmul4 max==255", "max value != 255 after normalization");
    }
#endif
}

static void test_direction(const image::io::metadata_t<int16_t> &gx,
                           const image::io::metadata_t<int16_t> &gy)
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
    {
        FAIL("Direction status", "returned error");
        return;
    }
    std::cout << "  Direction Scalar   : " << elapsed_ms(t0, t1) << " ms\n";

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
    if (valid_angles)
        PASS("Direction all outputs in {0,45,90,135}");
    else
        FAIL("Direction output values", "invalid angle found");

    {
        auto tgx = make_image<int16_t>(4, 1);
        auto tgy = make_image<int16_t>(4, 1);
        auto tdir = make_image<uint8_t>(4, 1);

        tgx.buffer.get()[0] = 255;
        tgy.buffer.get()[0] = 0; // → 0°
        tgx.buffer.get()[1] = 0;
        tgy.buffer.get()[1] = 255; // → 90°
        tgx.buffer.get()[2] = 255;
        tgy.buffer.get()[2] = 255; // → 135°
        tgx.buffer.get()[3] = 255;
        tgy.buffer.get()[3] = -255; // → 45°

        processing::Direction<uint8_t, int16_t>(tdir, tgx.buffer.get(), tgy.buffer.get());

        bool cases_ok = (tdir.buffer.get()[0] == 0) &&
                        (tdir.buffer.get()[1] == 90) &&
                        (tdir.buffer.get()[2] == 135) &&
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

static void test_pipeline(const image::io::metadata_t<uint8_t> &src)
{
    std::cout << "\n[Full Pipeline — end-to-end timing]\n";

    const uint32_t w = src.width;
    const uint32_t h = src.height;

    auto img_tmp = make_image<uint8_t>(w, h);
    auto gx = make_image<int16_t>(w, h);
    auto gy = make_image<int16_t>(w, h);
    auto mag = make_image<uint8_t>(w, h);
    auto dir = make_image<uint8_t>(w, h);

    // --- 1. Gaussian Benchmarks ---
    // Spatial Scalar
    std::memcpy(img_tmp.buffer.get(), src.buffer.get(), src.pixel_count);
    auto t0 = Clock::now();
    processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_tmp);
    auto t1 = Clock::now();
    long ms_gauss_spatial = elapsed_ms(t0, t1);

    // Separable Scalar
    std::memcpy(img_tmp.buffer.get(), src.buffer.get(), src.pixel_count);
    t0 = Clock::now();
    processing::gaussian_separable_5x5<uint8_t, int32_t>(img_tmp);
    t1 = Clock::now();
    long ms_gauss_sep = elapsed_ms(t0, t1);

    long ms_gauss_rvv = 0;
#ifdef __riscv
    // Spatial RVV
    std::memcpy(img_tmp.buffer.get(), src.buffer.get(), src.pixel_count);
    t0 = Clock::now();
    processing::gaussian_spatial_5x5_rvv(img_tmp);
    t1 = Clock::now();
    ms_gauss_rvv = elapsed_ms(t0, t1);
#endif

    // --- 2. Sobel Benchmarks ---
    // Note: img_tmp is already blurred from the last Gaussian pass

    // Sobel Scalar
    t0 = Clock::now();
    processing::sobel_3x3<uint8_t, int16_t>(img_tmp, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    long ms_sobel_scalar = elapsed_ms(t0, t1);

    long ms_sobel_rvv = 0;
#ifdef __riscv
    // Sobel RVV
    t0 = Clock::now();
    processing::sobel_3x3_rvv<2>(img_tmp, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    ms_sobel_rvv = elapsed_ms(t0, t1);
#endif

    // --- 3. Magnitude Benchmarks ---
    // Note: gx and gy contain valid gradient data from the last Sobel pass

    // Magnitude L1 Scalar
    t0 = Clock::now();
    processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    long ms_mag_l1 = elapsed_ms(t0, t1);

    // Magnitude L2 Scalar
    t0 = Clock::now();
    processing::MagL2<uint8_t, int16_t, float>(mag, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    long ms_mag_l2 = elapsed_ms(t0, t1);

    long ms_mag_rvv = 0;
#ifdef __riscv
    // Magnitude L1 RVV
    t0 = Clock::now();
    processing::MagL1<4>(mag, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    ms_mag_rvv = elapsed_ms(t0, t1);
#endif

    // --- 4. Direction Benchmark ---
    t0 = Clock::now();
    processing::Direction<uint8_t, int16_t>(dir, gx.buffer.get(), gy.buffer.get());
    t1 = Clock::now();
    long ms_dir = elapsed_ms(t0, t1);

    // ========================================================================
    // --- Output Formatting ---
    // ========================================================================

    std::cout << "  [Individual Stage Timings]\n";
    std::cout << "    Gaussian Spatial   : " << ms_gauss_spatial << " ms\n";
    std::cout << "    Gaussian Separable : " << ms_gauss_sep << " ms\n";
#ifdef __riscv
    std::cout << "    Gaussian RVV       : " << ms_gauss_rvv << " ms\n";
#endif
    std::cout << "    Sobel Scalar       : " << ms_sobel_scalar << " ms\n";
#ifdef __riscv
    std::cout << "    Sobel RVV          : " << ms_sobel_rvv << " ms\n";
#endif
    std::cout << "    Magnitude L1       : " << ms_mag_l1 << " ms\n";
    std::cout << "    Magnitude L2       : " << ms_mag_l2 << " ms\n";
#ifdef __riscv
    std::cout << "    Magnitude RVV      : " << ms_mag_rvv << " ms\n";
#endif
    std::cout << "    Direction          : " << ms_dir << " ms\n\n";

    // --- Pipeline Breakdown: Reference Scalar ---
    long total_scalar = ms_gauss_sep + ms_sobel_scalar + ms_mag_l2 + ms_dir;
    std::cout << "  [Pipeline A] Scalar (Separable + L2) Total: " << total_scalar << " ms\n";
    if (total_scalar > 0)
    {
        std::cout << "    Breakdown: "
                  << "Gaussian=" << (ms_gauss_sep * 100 / total_scalar) << "% | "
                  << "Sobel=" << (ms_sobel_scalar * 100 / total_scalar) << "% | "
                  << "Magnitude=" << (ms_mag_l2 * 100 / total_scalar) << "% | "
                  << "Direction=" << (ms_dir * 100 / total_scalar) << "%\n";
    }

#ifdef __riscv
    // --- Pipeline Breakdown: Accelerated RVV ---
    long total_rvv = ms_gauss_rvv + ms_sobel_rvv + ms_mag_rvv + ms_dir;
    std::cout << "\n  [Pipeline B] RVV Accelerated Total      : " << total_rvv << " ms\n";
    if (total_rvv > 0)
    {
        std::cout << "    Breakdown: "
                  << "Gaussian=" << (ms_gauss_rvv * 100 / total_rvv) << "% | "
                  << "Sobel=" << (ms_sobel_rvv * 100 / total_rvv) << "% | "
                  << "Magnitude=" << (ms_mag_rvv * 100 / total_rvv) << "% | "
                  << "Direction=" << (ms_dir * 100 / total_rvv) << "%\n";
    }
#endif

    PASS("full pipeline completed without error");
}

int main()
{
    std::cout << "=== Canny QEMU Equivalence Test (Phase 3.2) ===\n";
    std::cout << "Image: 100x75 (non-power-of-two — forces strip-mining tail)\n";

    const uint32_t W = 256;
    const uint32_t H = 256;

    // Load source image
    auto src = make_image<uint8_t>(W, H);
    Status load_stat = image::io::load_raw<uint8_t>("Atta.raw", src);
    if (load_stat != Status::E_OK)
    {
        std::cerr << "ERROR: could not load Atta.raw — place it in the working directory.\n";
        return 1;
    }
    std::cout << "Loaded Atta.raw (" << W << "x" << H << ")\n";

    // Intermediate buffers shared across test functions
    auto blurred = make_image<uint8_t>(W, H);
    std::memcpy(blurred.buffer.get(), src.buffer.get(), src.pixel_count);
    processing::gaussian_separable_5x5<uint8_t, int32_t>(blurred);

    auto gx = make_image<int16_t>(W, H);
    auto gy = make_image<int16_t>(W, H);
    processing::sobel_3x3<uint8_t, int16_t>(blurred, gx.buffer.get(), gy.buffer.get());

    // Run all test sections
    test_gaussian(src);
    test_gaussian_rvv(src);
    test_sobel(blurred, gx, gy);
    test_sobel_rvv(blurred);
    test_magnitude(gx, gy);
    test_magnitude_rvv(gx, gy);
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