/**
 * @file sobel_test.cpp
 * @brief sobel functionality test on QEMU
 */

#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "sobel.hpp"
#include "utils.hpp"

int main()
{
    Status stat;
    const uint32_t width  = 100;
    const uint32_t height = 75;
    const size_t pixel_count = width * height;

    const size_t aligned_size_u8  = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // Input: blurred grayscale image
    image::io::metadata_t<uint8_t> img_input;
    img_input.width               = width;
    img_input.height              = height;
    img_input.pixel_count         = pixel_count;
    img_input.aligned_buffer_size = aligned_size_u8;
    img_input.buffer.reset(static_cast<uint8_t*>(
        utils::memory::aligned_alloc(64, aligned_size_u8)));

    stat = image::io::load_raw<uint8_t>("rect.raw", img_input);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to load rect.raw!" << std::endl;
        return static_cast<int>(stat);
    }

    // Output: Gx and Gy gradient buffers (int16_t — max Sobel value is ±2040)
    image::io::metadata_t<int16_t> img_gx;
    img_gx.width               = width;
    img_gx.height              = height;
    img_gx.pixel_count         = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    img_gx.buffer.reset(static_cast<int16_t*>(
        utils::memory::aligned_alloc(64, aligned_size_i16)));

    image::io::metadata_t<int16_t> img_gy;
    img_gy.width               = width;
    img_gy.height              = height;
    img_gy.pixel_count         = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    img_gy.buffer.reset(static_cast<int16_t*>(
        utils::memory::aligned_alloc(64, aligned_size_i16)));

    auto start = std::chrono::high_resolution_clock::now();

    stat = processing::sobel_3x3<uint8_t, int16_t>(
        img_input, img_gx.buffer.get(), img_gy.buffer.get());

    auto end = std::chrono::high_resolution_clock::now();

    if (Status::E_OK != stat) {
        std::cerr << "Sobel failed! Error: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Sobel Processing Time: " << dur.count() << " ms" << std::endl;

    stat = image::io::save_raw<int16_t>("rect_gx.raw", img_gx);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to save rect_gx.raw!" << std::endl;
        return static_cast<int>(stat);
    }

    stat = image::io::save_raw<int16_t>("rect_gy.raw", img_gy);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to save rect_gy.raw!" << std::endl;
        return static_cast<int>(stat);
    }

    std::cout << "Sobel test completed and saved successfully!" << std::endl;
    return 0;
}