#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <gtest/gtest.h>
#include "std_types.hpp"
#include "io.hpp"
#include "sobel.hpp"
#include "utils.hpp"

TEST(SobelTest, Sobel3x3)
{
    Status stat;
    const uint32_t width = 512;
    const uint32_t height = 512;
    const size_t pixel_count = width * height;

    const size_t aligned_size_u8 = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));

    image::io::metadata_t<uint8_t> img_input;
    img_input.width = width;
    img_input.height = height;
    img_input.pixel_count = pixel_count;
    img_input.aligned_buffer_size = aligned_size_u8;
    img_input.buffer.reset(static_cast<uint8_t *>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    image::io::metadata_t<int16_t> img_gx;
    img_gx.width = width;
    img_gx.height = height;
    img_gx.pixel_count = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    img_gx.buffer.reset(static_cast<int16_t *>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    image::io::metadata_t<int16_t> img_gy;
    img_gy.width = width;
    img_gy.height = height;
    img_gy.pixel_count = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    img_gy.buffer.reset(static_cast<int16_t *>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    stat = image::io::load_raw<uint8_t>("vertical_edge.raw", img_input);

    ASSERT_EQ(Status::E_OK, stat) << "Failed to load vertical_edge.raw! Error code: " << static_cast<int>(stat);

    auto start = std::chrono::high_resolution_clock::now();
    stat = processing::sobel_3x3(img_input, img_gx, img_gy);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_EQ(Status::E_OK, stat) << "Sobel execution failed! Error code: " << static_cast<int>(stat);

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Sobel 3x3 Processing Time: " << dur.count() << " ms" << std::endl;

    stat = image::io::save_raw<int16_t>("vertical_edge_gx.raw", img_gx);
    ASSERT_EQ(Status::E_OK, stat) << "Failed to save vertical_edge_gx.raw!";

    stat = image::io::save_raw<int16_t>("vertical_edge_gy.raw", img_gy);
    ASSERT_EQ(Status::E_OK, stat) << "Failed to save vertical_edge_gy.raw!";

    std::cout << "Sobel test completed and saved successfully!" << std::endl;
}