/**
 * @file    image_types.hpp
 * @brief   Shared types, status codes, and image metadata for the Canny pipeline.
 *          All pipeline stages use these types for consistency.
 * @author  Youssef
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>

// -----------------------------------------------------------------------------
// Status codes — returned by all pipeline functions instead of bool
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Image metadata — bundles buffer, dimensions, and size info together
// -----------------------------------------------------------------------------

/**
 * @brief   Image metadata struct templated on pixel type.
 *
 * Owns the image buffer via RAII (std::unique_ptr with free deleter).
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
    std::unique_ptr<PixelT[], decltype(&std::free)> buffer {nullptr, std::free};

    int    width               = 0;   ///< Image width in pixels
    int    height              = 0;   ///< Image height in pixels
    size_t pixel_count         = 0;   ///< Total number of pixels (width * height)
    size_t aligned_buffer_size = 0;   ///< Actual allocated size in bytes (padded to 64 bytes)

    /**
     * @brief Construct metadata with known dimensions (buffer allocated later by load_raw).
     * @param w Image width in pixels.
     * @param h Image height in pixels.
     */
    metadata_t(int w, int h) : width(w), height(h) {}

    /// Non-copyable — owns the buffer
    metadata_t(const metadata_t&)            = delete;
    metadata_t& operator=(const metadata_t&) = delete;

    /// Movable
    metadata_t(metadata_t&&)            = default;
    metadata_t& operator=(metadata_t&&) = default;
};
