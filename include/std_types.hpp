/**
 * @file std_types.hpp
 * @brief Standard type definitions and common status codes.
 */

#pragma once

#include "utils.hpp"
#include <cstddef>
#include <memory>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Status codes — returned by all pipeline functions instead of bool
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief   Return status codes for all pipeline operations.
 */
enum class Status : uint8_t
{
    E_OK          = 0,   ///< Operation succeeded
    E_NOK         = 1,   ///< General failure
    E_ALLOC_FAIL  = 2,   ///< Memory allocation failed
    E_INVAL_PTR   = 3,   ///< Null or invalid pointer passed
    E_INVAL_DIR   = 4,   ///< File path or directory not found
    E_INVAL_SIZE  = 5,   ///< Invalid width or height (zero or negative)
    E_READ_FAIL   = 6,   ///< File read did not return expected bytes
    E_WRITE_FAIL  = 7,   ///< File write failed
};

// ─────────────────────────────────────────────────────────────────────────────
// Image metadata — bundles buffer, dimensions, and size info together
// ─────────────────────────────────────────────────────────────────────────────

namespace image::io
{

/**
 * @brief   Image metadata struct templated on pixel type.
 *
 * Owns the image buffer via RAII (std::unique_ptr with custom deleter).
 * All pipeline stages receive and pass this struct instead of raw pointers.
 *
 * @tparam  PixelT  Pixel component type. Default: uint8_t (grayscale 0-255).
 *                  Use int16_t for Sobel gradient buffers (signed, wider range).
 *                  Use int32_t for accumulator buffers during convolution.
 */
template <typename PixelT = uint8_t>
struct metadata_t
{
    /// RAII buffer — automatically freed when metadata_t goes out of scope
    std::unique_ptr<PixelT[], utils::memory::deleter> buffer;

    uint32_t width               = 0;   ///< Image width in pixels
    uint32_t height              = 0;   ///< Image height in pixels
    size_t   pixel_count         = 0;   ///< Total number of pixels (width * height)
    size_t   aligned_buffer_size = 0;   ///< Actual allocated size in bytes (padded to 64 bytes)

    /// Movable
    metadata_t()            = default;
    metadata_t(metadata_t&&) = default;
    metadata_t& operator=(metadata_t&&) = default;

    /// Non-copyable — owns the buffer
    metadata_t(const metadata_t&)            = delete;
    metadata_t& operator=(const metadata_t&) = delete;
};

} // namespace image::io

// ─────────────────────────────────────────────────────────────────────────────
// Kernel type alias — int16_t coefficients for integer convolution
// ─────────────────────────────────────────────────────────────────────────────

/// 5x5 convolution kernel stored in row-major order
using Kernel5x5 = int16_t[5][5];

/// 1x5 separable kernel
using Kernel1x5 = int16_t[5];

namespace kernels
{

/// 5x5 Gaussian kernel, sigma≈1.0, integer coefficients summing to 273
constexpr Kernel5x5 GAUSSIAN_5X5 = {
    { 1,  4,  7,  4,  1},
    { 4, 16, 26, 16,  4},
    { 7, 26, 41, 26,  7},
    { 4, 16, 26, 16,  4},
    { 1,  4,  7,  4,  1}
};

/// 1x5 Gaussian kernel for separable implementation, sum = 17
constexpr Kernel1x5 GAUSSIAN_1X5 = {1, 4, 7, 4, 1};

/// Normalization divisor for 5x5 kernel
constexpr int32_t GAUSSIAN_NORM   = 273;

} // namespace kernels
