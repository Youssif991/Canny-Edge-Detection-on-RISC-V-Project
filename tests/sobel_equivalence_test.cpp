#include <iostream>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <memory>
#include <algorithm>
#include "std_types.hpp"
#include "io.hpp"
#include "sobel.hpp"
#include "utils.hpp"

// Standalone Scalar Baseline for isolated side-by-side validation
template <typename PixelT = uint8_t, typename OutputT = int16_t>
Status sobel_3x3_scalar(
    const image::io::metadata_t<PixelT> &input,
    image::io::metadata_t<OutputT> &gx,
    image::io::metadata_t<OutputT> &gy)
{
    if (!input.height || !input.width || !input.buffer) return Status::E_INVAL_PTR;

    const int32_t W = static_cast<int32_t>(input.width);
    const int32_t H = static_cast<int32_t>(input.height);

    for (int32_t y = 0; y < H; ++y) {
        const int32_t y_top = std::clamp(y - 1, 0, H - 1);
        const int32_t y_bot = std::clamp(y + 1, 0, H - 1);

        const PixelT *row_top = &input.buffer.get()[y_top * W];
        const PixelT *row_mid = &input.buffer.get()[y * W];
        const PixelT *row_bot = &input.buffer.get()[y_bot * W];

        OutputT *out_gx = &gx.buffer.get()[y * W];
        OutputT *out_gy = &gy.buffer.get()[y * W];

        for (int32_t x = 0; x < W; ++x) {
            const int32_t x_l = std::clamp(x - 1, 0, W - 1);
            const int32_t x_r = std::clamp(x + 1, 0, W - 1);

            // Gx computation
            out_gx[x] = static_cast<OutputT>(
                (row_top[x_r] - row_top[x_l]) +
                ((row_mid[x_r] - row_mid[x_l]) << 1) +
                (row_bot[x_r] - row_bot[x_l]));

            // Gy computation
            out_gy[x] = static_cast<OutputT>(
                (row_top[x_l] + (row_top[x] << 1) + row_top[x_r]) -
                (row_bot[x_l] + (row_bot[x] << 1) + row_bot[x_r]));
        }
    }
    return Status::E_OK;
}

void run_sobel_equivalence_test() {
    // 1. Define non-power-of-two dimensions to force strip-mining tail cases
    const uint32_t width = 101;
    const uint32_t height = 73;
    const size_t pixel_count = width * height;
    const size_t aligned_size_8bit = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_16bit = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // 2. Set up Input metadata
    image::io::metadata_t<uint8_t> img_input;
    img_input.width = width; img_input.height = height; img_input.pixel_count = pixel_count;
    img_input.aligned_buffer_size = aligned_size_8bit;
    void* raw_input = utils::memory::aligned_alloc(64, aligned_size_8bit);
    img_input.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_input));

    // 3. Set up Scalar Baseline structures
    image::io::metadata_t<int16_t> gx_scalar;
    gx_scalar.width = width; gx_scalar.height = height; gx_scalar.pixel_count = pixel_count;
    gx_scalar.aligned_buffer_size = aligned_size_16bit;
    void* raw_gx_scalar = utils::memory::aligned_alloc(64, aligned_size_16bit);
    gx_scalar.buffer = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gx_scalar));

    image::io::metadata_t<int16_t> gy_scalar;
    gy_scalar.width = width; gy_scalar.height = height; gy_scalar.pixel_count = pixel_count;
    gy_scalar.aligned_buffer_size = aligned_size_16bit;
    void* raw_gy_scalar = utils::memory::aligned_alloc(64, aligned_size_16bit);
    gy_scalar.buffer = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gy_scalar));

    // 4. Set up Vector (RVV) pipeline structures
    image::io::metadata_t<int16_t> gx_vector;
    gx_vector.width = width; gx_vector.height = height; gx_vector.pixel_count = pixel_count;
    gx_vector.aligned_buffer_size = aligned_size_16bit;
    void* raw_gx_vector = utils::memory::aligned_alloc(64, aligned_size_16bit);
    gx_vector.buffer = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gx_vector));

    image::io::metadata_t<int16_t> gy_vector;
    gy_vector.width = width; gy_vector.height = height; gy_vector.pixel_count = pixel_count;
    gy_vector.aligned_buffer_size = aligned_size_16bit;
    void* raw_gy_vector = utils::memory::aligned_alloc(64, aligned_size_16bit);
    gy_vector.buffer = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gy_vector));

    // 5. Fill input with a deterministic synthetic test pattern
    for (size_t i = 0; i < pixel_count; i++) {
        img_input.buffer.get()[i] = static_cast<uint8_t>(i % 256);
    }

    // 6. Run isolated Scalar baseline
    Status stat_scalar = sobel_3x3_scalar<uint8_t, int16_t>(img_input, gx_scalar, gy_scalar);
    assert(Status::E_OK == stat_scalar && "Local Scalar Sobel execution failed!");

    // 7. Run target architecture pipeline (Vector/RVV from linked codebase)
    Status stat_vector = processing::sobel_3x3<uint8_t, int16_t>(img_input, gx_vector, gy_vector);
    assert(Status::E_OK == stat_vector && "Target Sobel execution failed!");

    // 8. Cross-examine every single coordinate pair
    for (size_t i = 0; i < pixel_count; i++) {
        int diff_x = std::abs(static_cast<int>(gx_scalar.buffer.get()[i]) - static_cast<int>(gx_vector.buffer.get()[i]));
        int diff_y = std::abs(static_cast<int>(gy_scalar.buffer.get()[i]) - static_cast<int>(gy_vector.buffer.get()[i]));
        
        if (diff_x > 1 || diff_y > 1) {
            std::cerr << "❌ Sobel mismatch at pixel index " << i 
                      << " (Row: " << (i / width) << ", Col: " << (i % width) << ")" << std::endl;
            std::cerr << "   -> Gx Scalar: " << static_cast<int>(gx_scalar.buffer.get()[i]) 
                      << " | Gx Vector: " << static_cast<int>(gx_vector.buffer.get()[i]) << std::endl;
            std::cerr << "   -> Gy Scalar: " << static_cast<int>(gy_scalar.buffer.get()[i]) 
                      << " | Gy Vector: " << static_cast<int>(gy_vector.buffer.get()[i]) << std::endl;
            assert(false);
        }
    }

    std::cout << "✅ Sobel Equivalence Test PASSED for this VLEN configuration!" << std::endl;
}

int main() {
    std::cout << "Starting Phase 3.2 Sobel Equivalence Verification on QEMU..." << std::endl;
    run_sobel_equivalence_test();
    return 0;
}