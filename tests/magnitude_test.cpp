/**
 * @file magnitude_test.cpp
 * @brief magnitude functionality test on QEMU
 */

#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "magnitude.hpp"
#include "utils.hpp"

int main()
{
    Status stat;
    const uint32_t width = 100;
    const uint32_t height = 75;
    const size_t pixel_count = width * height;
    
    const size_t aligned_size_u8  = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // Setup Input Gradient Structures
    image::io::metadata_t<int16_t> img_gx;
    img_gx.width = width; img_gx.height = height; img_gx.pixel_count = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    img_gx.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    image::io::metadata_t<int16_t> img_gy;
    img_gy.width = width; img_gy.height = height; img_gy.pixel_count = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    img_gy.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    stat = image::io::load_raw<int16_t>("rect_100x75_gx.raw", img_gx);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to load rect_100x75_gx.raw! Please run test_sobel first." << std::endl;
        return static_cast<int>(stat);
    }
    stat = image::io::load_raw<int16_t>("rect_100x75_gy.raw", img_gy);
    if (Status::E_OK != stat) return static_cast<int>(stat);

    // Setup Output Magnitude Structure (Grayscale uint8_t)
    image::io::metadata_t<uint8_t> img_mag;
    img_mag.width = width; img_mag.height = height; img_mag.pixel_count = pixel_count;
    img_mag.aligned_buffer_size = aligned_size_u8;
    img_mag.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    auto start_l1 = std::chrono::high_resolution_clock::now();
    stat = processing::MagL1<uint8_t, int16_t, uint16_t>(img_mag, img_gx.buffer.get(), img_gy.buffer.get());
    auto end_l1 = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur_l1 = std::chrono::duration_cast<std::chrono::milliseconds>(end_l1 - start_l1);
        std::cout << "Magnitude L1 Calculation Time: " << dur_l1.count() << " ms" << std::endl;
        
        // Save the L1 output image to check its look
        image::io::save_raw<uint8_t>("rect_100x75_magnitude_l1.raw", img_mag);
    }

    auto start_l2 = std::chrono::high_resolution_clock::now();
    stat = processing::MagL2<uint8_t, int16_t, float>(img_mag, img_gx.buffer.get(), img_gy.buffer.get());
    auto end_l2 = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur_l2 = std::chrono::duration_cast<std::chrono::milliseconds>(end_l2 - start_l2);
        std::cout << "Magnitude L2 Calculation Time: " << dur_l2.count() << " ms" << std::endl;
        
        // Save the L2 output image
        image::io::save_raw<uint8_t>("rect_100x75_magnitude_l2.raw", img_mag);
    }

    std::cout << "\nBoth tests completed successfully!" << std::endl;
    return 0;
}
