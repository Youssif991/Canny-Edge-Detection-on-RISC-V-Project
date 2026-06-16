#include <iostream>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <memory>
#include <algorithm>
#include "std_types.hpp"
#include "io.hpp"
#include "magnitude.hpp"
#include "utils.hpp"

// Local Scalar Baselines mirroring your exact src/magnitude.cpp dynamic normalization
template <typename PixelT = uint8_t, typename GradientT = int16_t, typename MagnitudeT = uint16_t>
Status MagL1_scalar(const image::io::metadata_t<PixelT>& image, const GradientT* Gx, const GradientT* Gy) {
    if (!image.buffer || !Gx || !Gy) return Status::E_INVAL_PTR;
    
    const uint32_t pixel_count = image.pixel_count;
    auto magnitude_buffer = std::make_unique<MagnitudeT[]>(pixel_count);
    
    MagnitudeT max_magnitude = 0;
    for (uint32_t i = 0; i < pixel_count; ++i) {
        MagnitudeT raw_mag = static_cast<MagnitudeT>(std::abs(Gx[i]) + std::abs(Gy[i]));
        magnitude_buffer[i] = raw_mag;
        if (raw_mag > max_magnitude) {
            max_magnitude = raw_mag;
        }
    }
    
    for (uint32_t i = 0; i < pixel_count; ++i) {
        if (max_magnitude == 0) image.buffer.get()[i] = 0;
        else image.buffer.get()[i] = static_cast<PixelT>((magnitude_buffer[i] * 255) / max_magnitude);
    }
    return Status::E_OK;
}

template <typename PixelT = uint8_t, typename GradientT = int16_t, typename MagnitudeT = float>
Status MagL2_scalar(const image::io::metadata_t<PixelT>& image, const GradientT* Gx, const GradientT* Gy) {
    if (!image.buffer || !Gx || !Gy) return Status::E_INVAL_PTR;
    
    const uint32_t pixel_count = image.pixel_count;
    auto magnitude_buffer = std::make_unique<MagnitudeT[]>(pixel_count);
    
    MagnitudeT max_magnitude = 0.0f;
    for (uint32_t i = 0; i < pixel_count; ++i) {
        MagnitudeT raw_mag = std::sqrt(static_cast<MagnitudeT>(Gx[i] * Gx[i] + Gy[i] * Gy[i]));
        magnitude_buffer[i] = raw_mag;
        if (raw_mag > max_magnitude) {
            max_magnitude = raw_mag;
        }
    }
    
    for (uint32_t i = 0; i < pixel_count; ++i) {
        if (max_magnitude == 0.0f) image.buffer.get()[i] = 0;
        else image.buffer.get()[i] = static_cast<PixelT>((magnitude_buffer[i] * 255) / max_magnitude);
    }
    return Status::E_OK;
}

void run_magnitude_equivalence_test() {
    // Force a non-power-of-two image context to test strip-mining edge loop conditions
    const uint32_t width = 101;
    const uint32_t height = 73;
    const size_t pixel_count = width * height;
    const size_t aligned_size_8bit = utils::memory::align_64(pixel_count * sizeof(uint8_t));
    const size_t aligned_size_16bit = utils::memory::align_64(pixel_count * sizeof(int16_t));

    // Allocate high-speed 64-byte aligned buffers for synthetic input gradients
    void* raw_gx = utils::memory::aligned_alloc(64, aligned_size_16bit);
    void* raw_gy = utils::memory::aligned_alloc(64, aligned_size_16bit);
    auto Gx = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gx));
    auto Gy = std::unique_ptr<int16_t[], utils::memory::deleter>(static_cast<int16_t*>(raw_gy));

    // Populate input data with highly variable signed directional gradients
    for (size_t i = 0; i < pixel_count; i++) {
        Gx.get()[i] = static_cast<int16_t>((i % 100) - 50);
        Gy.get()[i] = static_cast<int16_t>((i % 80) - 40);
    }

    // -------------------------------------------------------------------------
    // TEST PART A: MagL1 (|Gx| + |Gy|) Equivalence
    // -------------------------------------------------------------------------
    image::io::metadata_t<uint8_t> img_l1_scalar;
    img_l1_scalar.width = width;
    img_l1_scalar.height = height;
    img_l1_scalar.pixel_count = pixel_count;
    img_l1_scalar.aligned_buffer_size = aligned_size_8bit;
    void* raw_mem_l1_s = utils::memory::aligned_alloc(64, aligned_size_8bit);
    img_l1_scalar.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_l1_s));

    image::io::metadata_t<uint8_t> img_l1_vector;
    img_l1_vector.width = width;
    img_l1_vector.height = height;
    img_l1_vector.pixel_count = pixel_count;
    img_l1_vector.aligned_buffer_size = aligned_size_8bit;
    void* raw_mem_l1_v = utils::memory::aligned_alloc(64, aligned_size_8bit);
    img_l1_vector.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_l1_v));

    assert(Status::E_OK == (MagL1_scalar<uint8_t, int16_t, uint16_t>(img_l1_scalar, Gx.get(), Gy.get())));
    assert(Status::E_OK == (processing::MagL1<uint8_t, int16_t, uint16_t>(img_l1_vector, Gx.get(), Gy.get())));

    for (size_t i = 0; i < pixel_count; i++) {
        int diff = std::abs(static_cast<int>(img_l1_scalar.buffer.get()[i]) - static_cast<int>(img_l1_vector.buffer.get()[i]));
        if (diff > 1) {
            std::cerr << "❌ MagL1 mismatch at index " << i << " (Scalar: " << (int)img_l1_scalar.buffer.get()[i] 
                      << " | Vector/Target: " << (int)img_l1_vector.buffer.get()[i] << ")" << std::endl;
            assert(false);
        }
    }
    std::cout << "✅ MagL1 Equivalence Test PASSED!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST PART B: MagL2 (sqrt(Gx^2 + Gy^2)) Equivalence
    // -------------------------------------------------------------------------
    image::io::metadata_t<uint8_t> img_l2_scalar;
    img_l2_scalar.width = width;
    img_l2_scalar.height = height;
    img_l2_scalar.pixel_count = pixel_count;
    img_l2_scalar.aligned_buffer_size = aligned_size_8bit;
    void* raw_mem_l2_s = utils::memory::aligned_alloc(64, aligned_size_8bit);
    img_l2_scalar.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_l2_s));

    image::io::metadata_t<uint8_t> img_l2_vector;
    img_l2_vector.width = width;
    img_l2_vector.height = height;
    img_l2_vector.pixel_count = pixel_count;
    img_l2_vector.aligned_buffer_size = aligned_size_8bit;
    void* raw_mem_l2_v = utils::memory::aligned_alloc(64, aligned_size_8bit);
    img_l2_vector.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(static_cast<uint8_t*>(raw_mem_l2_v));

    assert(Status::E_OK == (MagL2_scalar<uint8_t, int16_t, float>(img_l2_scalar, Gx.get(), Gy.get())));
    assert(Status::E_OK == (processing::MagL2<uint8_t, int16_t, float>(img_l2_vector, Gx.get(), Gy.get())));

    for (size_t i = 0; i < pixel_count; i++) {
        int diff = std::abs(static_cast<int>(img_l2_scalar.buffer.get()[i]) - static_cast<int>(img_l2_vector.buffer.get()[i]));
        if (diff > 1) {
            std::cerr << "❌ MagL2 mismatch at index " << i << " (Scalar: " << (int)img_l2_scalar.buffer.get()[i] 
                      << " | Vector/Target: " << (int)img_l2_vector.buffer.get()[i] << ")" << std::endl;
            assert(false);
        }
    }
    std::cout << "✅ MagL2 Equivalence Test PASSED for this VLEN configuration!" << std::endl;
}

int main() {
    std::cout << "Starting Phase 3.2 Magnitude Equivalence Verification on QEMU..." << std::endl;
    run_magnitude_equivalence_test();
    return 0;
}