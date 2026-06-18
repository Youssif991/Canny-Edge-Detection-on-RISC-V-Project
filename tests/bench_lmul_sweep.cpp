#include "gaussian_rvv.hpp"
#include "gaussian.hpp"
#include <iostream>
#include <chrono>
#include <memory>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

static long elapsed_ms(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main()
{
    const uint32_t width = 512;
    const uint32_t height = 512;
    const uint32_t size = width * height;

    std::cout << "========================================" << std::endl;
    ::std::cout << "LMUL Sweep Benchmark (512x512 Image)" << std::endl;
    std::cout << "========================================" << std::endl;

    // Create a dummy image
    auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    for (uint32_t i = 0; i < size; ++i) {
        buffer[i] = static_cast<uint8_t>(i % 256);
    }

    image::io::metadata_t<uint8_t> img_original;
    img_original.width = width;
    img_original.height = height;
    img_original.pixel_count = size;
    img_original.aligned_buffer_size = size;

    // Helper lambda to copy image and benchmark a function
    auto benchmark_func = [&](const std::string& name, auto func) {
        // Prepare fresh input
        image::io::metadata_t<uint8_t> img;
        img.width = width;
        img.height = height;
        img.pixel_count = size;
        img.aligned_buffer_size = size;
        img.buffer = std::unique_ptr<uint8_t[], utils::memory::deleter>(
            static_cast<uint8_t*>(utils::memory::aligned_alloc(64, size)),
            utils::memory::deleter()
        );
        std::copy_n(buffer.get(), size, img.buffer.get());

        // Run
        auto t0 = Clock::now();
        func(img);
        auto t1 = Clock::now();

        std::cout << name << ": \t" << elapsed_ms(t0, t1) << " ms\n";
    };

    // Benchmark scalar
    benchmark_func("Scalar (Original)  ", [](image::io::metadata_t<uint8_t>& img) {
        processing::gaussian_spatial_5x5<uint8_t, int32_t>(img);
    });

    // Benchmark LMUL configs
    benchmark_func("RVV Acc LMUL=2 (m2)", processing::gaussian_spatial_5x5_rvv_acc2);
    benchmark_func("RVV Acc LMUL=4 (m4)", processing::gaussian_spatial_5x5_rvv_acc4);
    benchmark_func("RVV Acc LMUL=8 (m8)", processing::gaussian_spatial_5x5_rvv_acc8);

    std::cout << "========================================" << std::endl;
    return 0;
}
