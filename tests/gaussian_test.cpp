/**
 * @file    gaussian_test.cpp
 * @brief   GoogleTest unit tests for the Gaussian blur pipeline stage.
 *          Tests both 2D convolution and separable filter implementations.
 *          Compiled natively with g++ — no RISC-V required.
 * @author  Youssef
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include "gaussian.hpp"
#include "image_utils.hpp"

// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------

static constexpr int W    = 100;
static constexpr int H    = 75;
static constexpr int SIZE = W * H;

/// Allocate a metadata_t with a 64-byte aligned buffer
static void alloc_meta(metadata_t<uint8_t>& m, int w, int h)
{
    const size_t size = static_cast<size_t>(w) * h;
    m.buffer.reset(static_cast<uint8_t*>(
        std::aligned_alloc(64, utils::memory::align_64(size))));
    m.pixel_count         = size;
    m.aligned_buffer_size = utils::memory::align_64(size);
}

// -----------------------------------------------------------------------------
// 2D convolution tests
// -----------------------------------------------------------------------------

TEST(Gaussian2D, UniformImageUnchanged)
{
    // Blurring a uniform image must return the same uniform value.
    // The hints guide allows ±1 tolerance for integer rounding.
    // Only interior pixels are checked — border pixels are affected
    // by zero-padding which intentionally darkens them slightly.
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    alloc_meta(input, W, H);
    alloc_meta(output, W, H);

    std::fill(input.buffer.get(), input.buffer.get() + SIZE, 128u);

    ASSERT_EQ(image::gaussian::blur_2d(input, output), Status::E_OK);

    for (int r = 2; r < H - 2; ++r)
        for (int c = 2; c < W - 2; ++c)
            EXPECT_NEAR(output.buffer[r * W + c], 128, 1)
                << "Failed at (" << r << "," << c << ")";
}

TEST(Gaussian2D, AllBlackStaysBlack)
{
    // Blurring an all-black image must produce all-black output.
    // No rounding tolerance needed — 0 * any_coefficient = 0.
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    alloc_meta(input, W, H);
    alloc_meta(output, W, H);

    std::fill(input.buffer.get(), input.buffer.get() + SIZE, 0u);

    ASSERT_EQ(image::gaussian::blur_2d(input, output), Status::E_OK);

    for (int i = 0; i < SIZE; ++i)
        EXPECT_EQ(output.buffer[i], 0u) << "Failed at index " << i;
}

TEST(Gaussian2D, ImpulseSpreadSymmetrically)
{
    // A single bright pixel (impulse) must spread to its neighbours
    // symmetrically — the Gaussian kernel is symmetric so the response
    // must be symmetric around the impulse location.
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    alloc_meta(input, W, H);
    alloc_meta(output, W, H);

    std::fill(input.buffer.get(), input.buffer.get() + SIZE, 0u);

    // Place impulse at centre, far from border to avoid padding effects
    const int cy = H / 2;
    const int cx = W / 2;
    input.buffer[cy * W + cx] = 255;

    ASSERT_EQ(image::gaussian::blur_2d(input, output), Status::E_OK);

    // Response must be symmetric: pixel above == pixel below,
    // pixel left == pixel right
    EXPECT_EQ(output.buffer[(cy - 1) * W + cx],
              output.buffer[(cy + 1) * W + cx])
        << "Vertical symmetry failed";

    EXPECT_EQ(output.buffer[cy * W + (cx - 1)],
              output.buffer[cy * W + (cx + 1)])
        << "Horizontal symmetry failed";
}

TEST(Gaussian2D, NullBufferReturnsError)
{
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    // Deliberately do not allocate buffers

    EXPECT_EQ(image::gaussian::blur_2d(input, output), Status::E_INVAL_PTR);
}

// -----------------------------------------------------------------------------
// Separable filter tests
// -----------------------------------------------------------------------------

TEST(GaussianSeparable, UniformImageUnchanged)
{
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    alloc_meta(input, W, H);
    alloc_meta(output, W, H);

    std::fill(input.buffer.get(), input.buffer.get() + SIZE, 128u);

    ASSERT_EQ(image::gaussian::blur_separable(input, output), Status::E_OK);

    for (int r = 2; r < H - 2; ++r)
        for (int c = 2; c < W - 2; ++c)
            EXPECT_NEAR(output.buffer[r * W + c], 128, 1)
                << "Failed at (" << r << "," << c << ")";
}

TEST(GaussianSeparable, AllBlackStaysBlack)
{
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> output(W, H);
    alloc_meta(input, W, H);
    alloc_meta(output, W, H);

    std::fill(input.buffer.get(), input.buffer.get() + SIZE, 0u);

    ASSERT_EQ(image::gaussian::blur_separable(input, output), Status::E_OK);

    for (int i = 0; i < SIZE; ++i)
        EXPECT_EQ(output.buffer[i], 0u) << "Failed at index " << i;
}

// -----------------------------------------------------------------------------
// Cross-implementation equivalence test
// -----------------------------------------------------------------------------

TEST(GaussianEquivalence, SeparableMatchesTwoD)
{
    // Both implementations must produce nearly identical output.
    // Allow ±2 tolerance — separable applies two separate /17 normalizations
    // whereas 2D applies one /273, causing small rounding differences.
    metadata_t<uint8_t> input(W, H);
    metadata_t<uint8_t> out_2d(W, H);
    metadata_t<uint8_t> out_sep(W, H);
    alloc_meta(input,  W, H);
    alloc_meta(out_2d, W, H);
    alloc_meta(out_sep, W, H);

    // Ramp pattern — exercises all pixel values
    for (int i = 0; i < SIZE; ++i)
        input.buffer[i] = static_cast<uint8_t>(i % 256);

    ASSERT_EQ(image::gaussian::blur_2d(input, out_2d),         Status::E_OK);
    ASSERT_EQ(image::gaussian::blur_separable(input, out_sep), Status::E_OK);

    for (int i = 0; i < SIZE; ++i)
        EXPECT_NEAR(out_2d.buffer[i], out_sep.buffer[i], 2)
            << "Mismatch at index " << i;
}