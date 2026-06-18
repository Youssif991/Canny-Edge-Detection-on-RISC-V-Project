#include <iostream>
#include <vector>
#include <cmath>
#include "gaussian.hpp"
#include "gaussian_rvv.hpp"

int main()
{
    const uint32_t width = 32;
    const uint32_t height = 32;
    const size_t total_pixels = width * height;

    image::io::metadata_t<uint8_t> img_scalar;
    img_scalar.width = width;
    img_scalar.height = height;
    img_scalar.pixel_count = total_pixels;
    img_scalar.aligned_buffer_size = utils::memory::align_64(total_pixels * sizeof(uint8_t));
    img_scalar.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, img_scalar.aligned_buffer_size)));

    image::io::metadata_t<uint8_t> img_rvv;
    img_rvv.width = width;
    img_rvv.height = height;
    img_rvv.pixel_count = total_pixels;
    img_rvv.aligned_buffer_size = utils::memory::align_64(total_pixels * sizeof(uint8_t));
    img_rvv.buffer.reset(static_cast<uint8_t*>(utils::memory::aligned_alloc(64, img_rvv.aligned_buffer_size)));

    // Initialize with a simple pattern
    for (size_t i = 0; i < total_pixels; ++i)
    {
        uint8_t val = (i % 255);
        img_scalar.buffer[i] = val;
        img_rvv.buffer[i] = val;
    }

    std::cout << "Running scalar..." << std::endl;
    processing::gaussian_spatial_5x5<uint8_t, int32_t>(img_scalar);

    std::cout << "Running RVV (acc4)..." << std::endl;
    processing::gaussian_spatial_5x5_rvv_acc4(img_rvv);

    std::cout << "Comparing results..." << std::endl;
    int mismatches = 0;
    int max_diff = 0;

    for (size_t i = 0; i < total_pixels; ++i)
    {
        int diff = std::abs((int)img_scalar.buffer[i] - (int)img_rvv.buffer[i]);
        if (diff > max_diff) max_diff = diff;
        if (diff > 1) // Allow up to 1 difference due to precision differences (fixed-point division)
        {
            mismatches++;
        }
    }

    if (mismatches > 0)
    {
        std::cout << "FAILED: " << mismatches << " mismatches out of " << total_pixels << std::endl;
        std::cout << "Max difference: " << max_diff << std::endl;
        return 1;
    }
    
    std::cout << "PASSED! Max difference: " << max_diff << std::endl;
    return 0;
}
