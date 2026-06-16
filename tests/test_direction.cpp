#include <chrono>
#include <iostream>
#include <cstdlib>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "direction.hpp"
#include "utils.hpp"

int main()
{
    Status stat;
    const uint32_t width = 512;
    const uint32_t height = 512;
    const size_t pixel_count = width * height;
    
    const size_t aligned_size_u8  = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_i16 = utils::memory::align_64(pixel_count * sizeof(int16_t));
    image::io::metadata_t<int16_t> img_gx;
    img_gx.width = width; img_gx.height = height; img_gx.pixel_count = pixel_count;
    img_gx.aligned_buffer_size = aligned_size_i16;
    img_gx.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));

    image::io::metadata_t<int16_t> img_gy;
    img_gy.width = width; img_gy.height = height; img_gy.pixel_count = pixel_count;
    img_gy.aligned_buffer_size = aligned_size_i16;
    img_gy.buffer.reset(static_cast<int16_t*>(utils::memory::aligned_alloc(64, aligned_size_i16)));
    stat = image::io::load_raw<int16_t>("rect_gx.raw", img_gx);
    if (Status::E_OK != stat) {
        std::cerr << "Failed to load rect_gx.raw! Please run test_sobel first." << std::endl;
        return static_cast<int>(stat);
    }

    stat = image::io::load_raw<int16_t>("rect_gy.raw", img_gy);
    if (Status::E_OK != stat) return static_cast<int>(stat);
    image::io::metadata_t<uint8_t> img_dir;
    img_dir.width = width;
    img_dir.height = height;
    img_dir.pixel_count = pixel_count;
    img_dir.aligned_buffer_size = aligned_size_u8;
    img_dir.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, aligned_size_u8)));
    auto start = std::chrono::high_resolution_clock::now();
    
    stat = processing::Direction<uint8_t, int16_t>(img_dir, img_gx.buffer.get(), img_gy.buffer.get());
    
    auto end = std::chrono::high_resolution_clock::now();

    if (Status::E_OK == stat) {
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Direction Processing Time: " << dur.count() << " ms" << std::endl;

        // Save output directly to root folder
        stat = image::io::save_raw<uint8_t>("rect_direction.raw", img_dir);
        if (Status::E_OK != stat) return static_cast<int>(stat);
        
        std::cout << "Direction test completed and saved successfully!" << std::endl;
    } else {
        std::cerr << "Direction calculation failed! Error: " << static_cast<int>(stat) << std::endl;
        return static_cast<int>(stat);
    }

    return 0;
}