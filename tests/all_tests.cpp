#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include "utils.hpp"

int main()
{
    Status stat;
    const uint32_t width = 512;
    const uint32_t height = 512;
    const size_t pixel_count = width * height;
    
    // Compute perfectly aligned buffer boundary allocations
    const size_t aligned_size_u8  = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // =========================================================================
    // 1. ALLOCATE METADATA STRUCTURES AND BUFFERS
    // =========================================================================
    
    // Original input image structure (8-bit unsigned)
    image::io::metadata_t<uint8_t> img_input;
    img_input.width = width; img_input.height = height; img_input.pixel_count = pixel_count;
    img_input.aligned_buffer_size = aligned_size_u8;
    img_input.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    // Intermediate Sobel Gx gradient structure (16-bit signed)
    image::io::metadata_t<int16_t> img_gx;
    img_gx.width = width; img_gx.height = height; img_gx.pixel_count = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    img_gx.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    // Intermediate Sobel Gy gradient structure (16-bit signed)
    image::io::metadata_t<int16_t> img_gy;
    img_gy.width = width; img_gy.height = height; img_gy.pixel_count = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    img_gy.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    // Output Magnitude structure (8-bit unsigned)
    image::io::metadata_t<uint8_t> img_mag;
    img_mag.width = width; img_mag.height = height; img_mag.pixel_count = pixel_count;
    img_mag.aligned_buffer_size = aligned_size_u8;
    img_mag.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    // Output Direction structure (8-bit unsigned for sector quantization)
    image::io::metadata_t<uint8_t> img_dir;
    img_dir.width = width; img_dir.height = height; img_dir.pixel_count = pixel_count;
    img_dir.aligned_buffer_size = aligned_size_u8;
    img_dir.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    // =========================================================================
    // 2. LOAD SOURCE IMAGE
    // =========================================================================
    stat = image::io::load_raw<uint8_t>("rect.raw", img_input);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to load rect.raw! Error code: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    std::cout << "--- Starting Integrated Canny Edge Pipeline --- \n" << std::endl;

    // =========================================================================
    // 3. EXECUTE STAGE 1: SOBEL FILTER (3x3)
    // =========================================================================
    auto start_sobel = std::chrono::high_resolution_clock::now();
    stat = processing::sobel_3x3(img_input, img_gx, img_gy);
    auto end_sobel = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end_sobel - start_sobel);
        std::cout << "[STAGE 1] Sobel 3x3 Processing Time: " << dur.count() << " ms" << std::endl;
        
        // Save gradients
        image::io::save_raw<int16_t>("rect_gx.raw", img_gx);
        image::io::save_raw<int16_t>("rect_gy.raw", img_gy);
    } else {
        std::cerr << "Sobel execution failed! Error code: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    // =========================================================================
    // 4. EXECUTE STAGE 2: MAGNITUDE BENCHMARK (L1 VS L2)
    // =========================================================================
    
    // Test Magnitude L1 Option
    auto start_l1 = std::chrono::high_resolution_clock::now();
    stat = processing::MagL1<uint8_t, int16_t, uint16_t>(img_mag, img_gx.buffer.get(), img_gy.buffer.get());
    auto end_l1 = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur_l1 = std::chrono::duration_cast<std::chrono::milliseconds>(end_l1 - start_l1);
        std::cout << "[STAGE 2] Magnitude L1 Processing Time: " << dur_l1.count() << " ms" << std::endl;
        image::io::save_raw<uint8_t>("rect_magnitude_l1.raw", img_mag);
    } else {
        std::cerr << "Magnitude L1 execution failed!" << std::endl;
        return static_cast<int>(stat);
    }

    // Test Magnitude L2 Option (Gold Standard)
    auto start_l2 = std::chrono::high_resolution_clock::now();
    stat = processing::MagL2<uint8_t, int16_t, float>(img_mag, img_gx.buffer.get(), img_gy.buffer.get());
    auto end_l2 = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur_l2 = std::chrono::duration_cast<std::chrono::milliseconds>(end_l2 - start_l2);
        std::cout << "[STAGE 2] Magnitude L2 Processing Time: " << dur_l2.count() << " ms" << std::endl;
        image::io::save_raw<uint8_t>("rect_magnitude_l2.raw", img_mag);
    } else {
        std::cerr << "Magnitude L2 execution failed!" << std::endl;
        return static_cast<int>(stat);
    }

    // =========================================================================
    // 5. EXECUTE STAGE 3: DIRECTION QUANTIZATION
    // =========================================================================
    auto start_dir = std::chrono::high_resolution_clock::now();
    stat = processing::Direction<uint8_t, int16_t>(img_dir, img_gx.buffer.get(), img_gy.buffer.get());
    auto end_dir = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur_dir = std::chrono::duration_cast<std::chrono::milliseconds>(end_dir - start_dir);
        std::cout << "[STAGE 3] Direction Processing Time:    " << dur_dir.count() << " ms" << std::endl;
        image::io::save_raw<uint8_t>("rect_direction.raw", img_dir);
    } else {
        std::cerr << "Direction execution failed! Error: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "All pipeline processing stages completed successfully!" << std::endl;
    return 0;
}