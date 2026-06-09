/**
 * @file    unit_tests.cpp
 * @brief   GoogleTest suite for raw image IO and Gaussian blur.
 */

#include <gtest/gtest.h>

#include "gaussian.hpp"
#include "io.hpp"
#include "utils.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cinttypes>

namespace
{

template <typename PixelT>
image::io::metadata_t<PixelT> allocate_image(uint32_t width, uint32_t height)
{
    image::io::metadata_t<PixelT> image;
    image.width = width;
    image.height = height;
    image.pixel_count = static_cast<size_t>(width) * height;
    image.aligned_buffer_size = utils::memory::align_64(image.pixel_count * sizeof(PixelT));
    image.buffer.reset(static_cast<PixelT*>(
        utils::memory::aligned_alloc(64, image.aligned_buffer_size)));
    EXPECT_NE(image.buffer, nullptr);
    return image;
}

template <typename PixelT>
void fill_uniform(image::io::metadata_t<PixelT>& image, PixelT value)
{
    std::memset(image.buffer.get(), value, image.pixel_count * sizeof(PixelT));
}

template <typename PixelT>
void fill_impulse(image::io::metadata_t<PixelT>& image, uint32_t x, uint32_t y, PixelT value)
{
    std::memset(image.buffer.get(), 0, image.pixel_count * sizeof(PixelT));
    image.buffer.get()[y * image.width + x] = value;
}

} // namespace

TEST(ImageIO, SaveLoadRoundTrip)
{
    const uint32_t width = 100;
    const uint32_t height = 75;
    const size_t pixel_count = static_cast<size_t>(width) * height;

    auto source = allocate_image<uint8_t>(width, height);
    for (size_t i = 0; i < pixel_count; ++i)
    {
        source.buffer.get()[i] = static_cast<uint8_t>(i % 256);
    }

    ASSERT_EQ(image::io::save_raw<uint8_t>("unit_tests_roundtrip.raw", source), Status::E_OK);

    auto loaded = allocate_image<uint8_t>(width, height);
    ASSERT_EQ(image::io::load_raw<uint8_t>("unit_tests_roundtrip.raw", loaded), Status::E_OK);

    for (size_t i = 0; i < pixel_count; ++i)
    {
        EXPECT_EQ(source.buffer.get()[i], loaded.buffer.get()[i]) << "Mismatch at index " << i;
    }

    std::remove("assets/unit_tests_roundtrip.raw");
}

TEST(ImageIO, InvalidDimensionsFail)
{
    image::io::metadata_t<uint8_t> image;
    image.width = 0;
    image.height = 0;

    EXPECT_EQ(image::io::load_raw<uint8_t>("unit_tests_invalid.raw", image), Status::E_INVAL_SIZE);
}

TEST(ImageIO, MissingFileFails)
{
    auto image = allocate_image<uint8_t>(64, 64);
    EXPECT_EQ(image::io::load_raw<uint8_t>("this_file_does_not_exist.raw", image), Status::E_INVAL_DIR);
}

TEST(ImageIO, BufferIs64ByteAlignedAfterLoad)
{
    auto source = allocate_image<uint8_t>(64, 64);
    fill_uniform(source, static_cast<uint8_t>(255));

    ASSERT_EQ(image::io::save_raw<uint8_t>("unit_tests_align.raw", source), Status::E_OK);

    auto loaded = allocate_image<uint8_t>(64, 64);
    ASSERT_EQ(image::io::load_raw<uint8_t>("unit_tests_align.raw", loaded), Status::E_OK);

    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(loaded.buffer.get()) % 64, 0u);

    std::remove("assets/unit_tests_align.raw");
}

TEST(Gaussian, UniformImage)
{
    const uint32_t dim = 128;

    auto spatial_image = allocate_image<uint8_t>(dim, dim);
    fill_uniform(spatial_image, static_cast<uint8_t>(128));

    auto separable_image = allocate_image<uint8_t>(dim, dim);
    fill_uniform(separable_image, static_cast<uint8_t>(128));

    ASSERT_EQ(processing::gaussian_spatial_5x5(spatial_image), Status::E_OK);
    ASSERT_EQ(processing::gaussian_separable_5x5(separable_image), Status::E_OK);

    for (uint32_t y = 2; y < dim - 2; ++y)
    {
        for (uint32_t x = 2; x < dim - 2; ++x)
        {
            const size_t idx = static_cast<size_t>(y) * dim + x;
            EXPECT_NEAR(spatial_image.buffer.get()[idx], 128, 1);
            EXPECT_NEAR(separable_image.buffer.get()[idx], 128, 1);
        }
    }
}

TEST(Gaussian, AllBlackImage)
{
    const uint32_t dim = 128;

    auto spatial_image = allocate_image<uint8_t>(dim, dim);
    fill_uniform(spatial_image, static_cast<uint8_t>(0));

    auto separable_image = allocate_image<uint8_t>(dim, dim);
    fill_uniform(separable_image, static_cast<uint8_t>(0));

    ASSERT_EQ(processing::gaussian_spatial_5x5(spatial_image), Status::E_OK);
    ASSERT_EQ(processing::gaussian_separable_5x5(separable_image), Status::E_OK);

    for (size_t i = 0; i < spatial_image.pixel_count; ++i)
    {
        EXPECT_EQ(spatial_image.buffer.get()[i], 0);
        EXPECT_EQ(separable_image.buffer.get()[i], 0);
    }
}

TEST(Gaussian, ImpulseSymmetry)
{
    const uint32_t dim = 128;
    const uint32_t cx = dim / 2;
    const uint32_t cy = dim / 2;

    auto spatial_image = allocate_image<uint8_t>(dim, dim);
    fill_impulse(spatial_image, cx, cy, static_cast<uint8_t>(255));

    auto separable_image = allocate_image<uint8_t>(dim, dim);
    fill_impulse(separable_image, cx, cy, static_cast<uint8_t>(255));

    ASSERT_EQ(processing::gaussian_spatial_5x5(spatial_image), Status::E_OK);
    ASSERT_EQ(processing::gaussian_separable_5x5(separable_image), Status::E_OK);

    const uint8_t* spatial = spatial_image.buffer.get();
    const uint8_t* separable = separable_image.buffer.get();

    EXPECT_EQ(spatial[cy * dim + (cx - 1)], spatial[cy * dim + (cx + 1)]);
    EXPECT_EQ(spatial[cy * dim + (cx - 2)], spatial[cy * dim + (cx + 2)]);
    EXPECT_EQ(spatial[(cy - 1) * dim + cx], spatial[(cy + 1) * dim + cx]);
    EXPECT_EQ(spatial[(cy - 2) * dim + cx], spatial[(cy + 2) * dim + cx]);

    EXPECT_EQ(separable[cy * dim + (cx - 1)], separable[cy * dim + (cx + 1)]);
    EXPECT_EQ(separable[cy * dim + (cx - 2)], separable[cy * dim + (cx + 2)]);
    EXPECT_EQ(separable[(cy - 1) * dim + cx], separable[(cy + 1) * dim + cx]);
    EXPECT_EQ(separable[(cy - 2) * dim + cx], separable[(cy + 2) * dim + cx]);

    EXPECT_GT(spatial[cy * dim + cx], 0);
    EXPECT_GT(separable[cy * dim + cx], 0);
}
