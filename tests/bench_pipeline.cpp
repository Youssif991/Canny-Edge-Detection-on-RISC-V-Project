#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include "io.hpp"
#include <ctime>
#include <cstdio>

static double wall_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    // Load image
    image::io::metadata_t<uint8_t> img;
    img.width = 512; img.height = 512;
    image::io::load_raw("rect.raw", img);

    // --- Gaussian ---
    double t0 = wall_ms();
    for (int i = 0; i < 120; ++i)
        processing::gaussian_separable_5x5<uint8_t, int32_t>(img);
    double t1 = wall_ms();
    const double t_gaussian = (t1 - t0) / 120.0;

    // --- Sobel ---
    image::io::metadata_t<int16_t> gx, gy;
    gx.width = gy.width = img.width;
    gx.height = gy.height = img.height;
    gx.pixel_count = gy.pixel_count = static_cast<size_t>(img.width) * img.height;
    gx.aligned_buffer_size = gy.aligned_buffer_size = utils::memory::align_64(
        gx.pixel_count * sizeof(int16_t));
    gx.buffer.reset(static_cast<int16_t *>(
        utils::memory::aligned_alloc(64, gx.aligned_buffer_size)));
    gy.buffer.reset(static_cast<int16_t *>(
        utils::memory::aligned_alloc(64, gy.aligned_buffer_size)));

    t0 = wall_ms();
    for (int i = 0; i < 120; ++i)
        processing::sobel_3x3<uint8_t, int16_t>(img, gx.buffer.get(), gy.buffer.get());
    t1 = wall_ms();
    const double t_sobel = (t1 - t0) / 120.0;

    // --- Magnitude ---
    image::io::metadata_t<uint8_t> mag;
    mag.width = img.width;
    mag.height = img.height;
    mag.pixel_count = static_cast<size_t>(img.width) * img.height;
    mag.aligned_buffer_size = utils::memory::align_64(mag.pixel_count * sizeof(uint8_t));
    mag.buffer.reset(static_cast<uint8_t *>(
        utils::memory::aligned_alloc(64, mag.aligned_buffer_size)));

    t0 = wall_ms();
    for (int i = 0; i < 120; ++i)
        processing::MagL1<uint8_t, int16_t, uint16_t>(mag, gx.buffer.get(), gy.buffer.get());
    t1 = wall_ms();
    const double t_magnitude = (t1 - t0) / 120.0;

    // --- Direction ---
    image::io::metadata_t<uint8_t> dir;
    dir.width = img.width;
    dir.height = img.height;
    dir.pixel_count = static_cast<size_t>(img.width) * img.height;
    dir.aligned_buffer_size = utils::memory::align_64(dir.pixel_count * sizeof(uint8_t));
    dir.buffer.reset(static_cast<uint8_t *>(
        utils::memory::aligned_alloc(64, dir.aligned_buffer_size)));

    t0 = wall_ms();
    for (int i = 0; i < 120; ++i)
        processing::Direction<uint8_t, int16_t>(dir, gx.buffer.get(), gy.buffer.get());
    t1 = wall_ms();
    const double t_direction = (t1 - t0) / 120.0;

    const double total_time = t_gaussian + t_sobel + t_magnitude + t_direction;
    const double p_gaussian = (t_gaussian / total_time) * 100.0;
    const double p_sobel = (t_sobel / total_time) * 100.0;
    const double p_magnitude = (t_magnitude / total_time) * 100.0;
    const double p_direction = (t_direction / total_time) * 100.0;

    printf("Stage           | Time (ms)    | Percent\n");
    printf("----------------|--------------|------------\n");
    printf("Gaussian        | %8.3f     | %6.2f%%\n", t_gaussian, p_gaussian);
    printf("Sobel Gx/Gy     | %8.3f     | %6.2f%%\n", t_sobel, p_sobel);
    printf("Magnitude       | %8.3f     | %6.2f%%\n", t_magnitude, p_magnitude);
    printf("Direction       | %8.3f     | %6.2f%%\n", t_direction, p_direction);
    printf("                |              |        \n");
    printf("--------------------------------------------\n");
    printf("\nBreakdown: Gaussian: %.1f%% | Sobel Gx/Gy: %.1f%% | Magnitude: %.1f%% | Direction: %.1f%%\n",
           p_gaussian, p_sobel, p_magnitude, p_direction);
}