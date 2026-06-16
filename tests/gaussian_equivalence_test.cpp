#include <iostream>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <memory>
#include "std_types.hpp"
#include "io.hpp"
#include "gaussian.hpp"
#include "utils.hpp"

void run_equivalence_test() {
    // 1. Define non-power-of-two dimensions to force the strip-mining tail case (Phase 3.2 requirement)
    const uint32_t width = 101;
    const uint32_t height = 73;
    const size_t pixel_count = width * height;
    const size_t aligned_size = utils::memory::align_64(pixel_count);

    // 2. Set up metadata and allocate buffer for Spatial (Scalar Baseline)
    image::io::metadata_t<uint8_t> img_spatial;
    img_spatial.width = width;
    img_spatial.height = height;
    img_spatial.pixel_count = pixel_count;
    img_spatial.aligned_buffer_size = aligned_size;
    
    // Allocate 64-byte aligned memory and wrap with custom deleter
    void* raw_mem_spatial = utils::memory::aligned_alloc(64, aligned_size);
    img_spatial.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_spatial));

    // 3. Set up metadata and allocate buffer for Separable (Optimized Vector/RVV)
    image::io::metadata_t<uint8_t> img_separable;
    img_separable.width = width;
    img_separable.height = height;
    img_separable.pixel_count = pixel_count;
    img_separable.aligned_buffer_size = aligned_size;
    
    // Allocate 64-byte aligned memory and wrap with custom deleter
    void* raw_mem_separable = utils::memory::aligned_alloc(64, aligned_size);
    img_separable.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_separable));

    // 4. Fill both input buffers with identical synthetic dummy noise patterns
    for (size_t i = 0; i < pixel_count; i++) {
        uint8_t dummy_pixel = static_cast<uint8_t>(i % 256);
        img_spatial.buffer[i]   = dummy_pixel;
        img_separable.buffer[i] = dummy_pixel;
    }

    // 5. Run the reference spatial 2D convolution pipeline (Scalar baseline)
    Status stat_spatial = processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_spatial);
    assert(Status::E_OK == stat_spatial && "Spatial Gaussian execution failed!");

    // 6. Run the optimized separable 1D horizontal/vertical pipeline (Your RVV implementation)
    Status stat_separable = processing::gaussian_separable_5x5<uint8_t, int32_t>(img_separable);
    assert(Status::E_OK == stat_separable && "Separable Gaussian execution failed!");

    // 7. Loop through every pixel to verify equivalence within a strict +/-1 tolerance
    for (size_t i = 0; i < pixel_count; i++) {
        int diff = std::abs(static_cast<int>(img_spatial.buffer[i]) - static_cast<int>(img_separable.buffer[i]));
        
        if (diff > 1) {
            std::cerr << " Equivalence failure at pixel index " << i 
                      << " (Row: " << (i / width) << ", Col: " << (i % width) << ")" << std::endl;
            std::cerr << "   -> Spatial (Scalar Baseline): " << static_cast<int>(img_spatial.buffer[i]) << std::endl;
            std::cerr << "   -> Separable (RVV Vector):    " << static_cast<int>(img_separable.buffer[i]) << std::endl;
            assert(false); // Immediate abort to flag VLA alignment or rounding bugs
        }
    }

    std::cout << " Equivalence Test PASSED for this VLEN configuration!" << std::endl;