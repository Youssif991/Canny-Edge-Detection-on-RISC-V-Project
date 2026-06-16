#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "sobel.hpp"
#include "utils.hpp"

// Custom deleter for unique_ptr to properly manage memory allocated via aligned_alloc
struct AlignedDeleter {
    void operator()(void* ptr) const {
        if (ptr) {
            std::free(ptr);
        }
    }
};

int main()
{
    Status stat;
    const uint32_t width = 512;
    const uint32_t height = 512;
    const size_t pixel_count = width * height;
    
    // Calculate aligned sizes (64-byte aligned boundary)
    const size_t aligned_size_u8  = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // 1. Setup and Allocate Input Structure
    image::io::metadata_t<uint8_t> img_input;
    img_input.width = width;
    img_input.height = height;
    img_input.pixel_count = pixel_count;
    img_input.aligned_buffer_size = aligned_size_u8;
    // CRITICAL: Allocate the physical heap memory buffer
    img_input.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));

    // 2. Setup and Allocate Gx Output Structure
    image::io::metadata_t<int16_t> img_gx;
    img_gx.width = width;
    img_gx.height = height;
    img_gx.pixel_count = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    // CRITICAL: Allocate the physical heap memory buffer
    img_gx.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    // 3. Setup and Allocate Gy Output Structure
    image::io::metadata_t<int16_t> img_gy;
    img_gy.width = width;
    img_gy.height = height;
    img_gy.pixel_count = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    // CRITICAL: Allocate the physical heap memory buffer
    img_gy.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    // Load the input raw image into the allocated buffer
    stat = image::io::load_raw<uint8_t>("rect.raw", img_input);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to load rect.raw! Error code: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    // Benchmarking the Sobel 3x3 Stage
    auto start = std::chrono::high_resolution_clock::now();
    stat = processing::sobel_3x3(img_input, img_gx, img_gy);
    auto end = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Sobel 3x3 Processing Time: " << dur.count() << " ms" << std::endl;

        // Save out Gx gradient image map
        stat = image::io::save_raw<int16_t>("rect_gx.raw", img_gx);
        if (Status::E_OK != stat) return static_cast<int>(stat);

        // Save out Gy gradient image map
        stat = image::io::save_raw<int16_t>("rect_gy.raw", img_gy);
        if (Status::E_OK != stat) return static_cast<int>(stat);
        
        std::cout << "Sobel test completed and saved successfully!" << std::endl;
    } else {
        std::cerr << "Sobel execution failed! Error code: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    return 0;
}