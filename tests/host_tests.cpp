#include <gtest/gtest.h>
#include "image_io.hpp"

// Non-power-of-two size as required by hints guide
static constexpr int W = 100;
static constexpr int H = 75;

TEST(ImageIO, LoadRawRoundTrip) {
    metadata_t<uint8_t> src(W, H);
    Status st = image::io::load_raw("test_temp.raw", src);
    
    // First create a file to load
    metadata_t<uint8_t> gen(W, H);
    const size_t pixel_count = static_cast<size_t>(W) * H;
    void* raw = std::aligned_alloc(64, utils::memory::align_64(pixel_count));
    gen.buffer.reset(static_cast<uint8_t*>(raw));
    gen.pixel_count = pixel_count;
    gen.aligned_buffer_size = utils::memory::align_64(pixel_count);
    
    for (size_t i = 0; i < pixel_count; i++)
        gen.buffer[i] = static_cast<uint8_t>(i % 256);

    ASSERT_EQ(image::io::save_raw("test_temp.raw", gen), Status::E_OK);
    
    metadata_t<uint8_t> dst(W, H);
    ASSERT_EQ(image::io::load_raw("test_temp.raw", dst), Status::E_OK);
    
    for (size_t i = 0; i < pixel_count; i++)
        EXPECT_EQ(gen.buffer[i], dst.buffer[i]) << "Mismatch at index " << i;

    std::remove("assets/test_temp.raw");
}

TEST(ImageIO, LoadNonExistentFails) {
    metadata_t<uint8_t> meta(W, H);
    EXPECT_EQ(image::io::load_raw("does_not_exist.raw", meta), Status::E_INVAL_DIR);
}

TEST(ImageIO, InvalidDimensionsFail) {
    metadata_t<uint8_t> meta(0, 0);
    EXPECT_EQ(image::io::load_raw("anything.raw", meta), Status::E_INVAL_SIZE);
}

TEST(ImageIO, BufferIs64ByteAligned) {
    metadata_t<uint8_t> gen(W, H);
    const size_t size = static_cast<size_t>(W) * H;
    void* raw = std::aligned_alloc(64, utils::memory::align_64(size));
    gen.buffer.reset(static_cast<uint8_t*>(raw));
    gen.pixel_count = size;
    gen.aligned_buffer_size = utils::memory::align_64(size);

    ASSERT_EQ(image::io::save_raw("align_test.raw", gen), Status::E_OK);

    metadata_t<uint8_t> dst(W, H);
    ASSERT_EQ(image::io::load_raw("align_test.raw", dst), Status::E_OK);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(dst.buffer.get()) % 64, 0u);

    std::remove("assets/align_test.raw");
}