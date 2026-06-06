/**
 * @file    image_io.hpp
 * @brief   Utilities for loading and saving raw grayscale image data.
 *
 * Raw format: exactly width * height bytes, one byte per pixel.
 * No headers, no compression — caller must supply width and height.
 * All buffers are 64-byte aligned for RVV vector load intrinsics (Phase 6)
 * and to help the compiler vectorize loads and stores.
 *
 * @author  Youssef
 */

#pragma once
#include "image_types.hpp"
#include "image_utils.hpp"
#include <string_view>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <fstream>

namespace image::io
{

/**
 * @brief   Load a raw binary image file from the assets directory into an
 *          aligned RAII buffer stored inside the metadata struct.
 *
 * Allocates 64-byte aligned memory via aligned_alloc to satisfy RISC-V
 * vector intrinsic requirements and cache line alignment.
 * Files are looked up relative to the ./assets/ directory.
 *
 * @tparam  PixelT      Pixel component type (default: uint8_t for grayscale).
 * @param   file_name   Name of the raw file to load (e.g. "input.raw").
 * @param   metadata    Metadata struct with width and height pre-filled.
 *                      Buffer, pixel_count, and aligned_buffer_size are
 *                      populated on success.
 * @return  Status::E_OK on success, or an error code on failure.
 */
template <typename PixelT = uint8_t>
[[nodiscard]] Status load_raw(std::string_view file_name,
                              metadata_t<PixelT>& metadata)
{
    if (metadata.width <= 0 || metadata.height <= 0)
    {
        return Status::E_INVAL_SIZE;
    }

    const size_t pixel_count         = static_cast<size_t>(metadata.width) * metadata.height;
    const size_t total_bytes         = pixel_count * sizeof(PixelT);
    const size_t aligned_buffer_size = utils::memory::align_64(total_bytes);

    // Allocate 64-byte aligned buffer
    void* raw_ptr = std::aligned_alloc(64, aligned_buffer_size);
    if (!raw_ptr)
    {
        return Status::E_ALLOC_FAIL;
    }

    // Transfer ownership into RAII unique_ptr — auto-freed on scope exit
    metadata.buffer.reset(static_cast<PixelT*>(raw_ptr));
    metadata.pixel_count         = pixel_count;
    metadata.aligned_buffer_size = aligned_buffer_size;

    std::filesystem::path file_path = std::filesystem::path("./assets/") / file_name;
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
    {
        return Status::E_INVAL_DIR;
    }

    if (!file.read(reinterpret_cast<char*>(metadata.buffer.get()), total_bytes))
    {
        return Status::E_READ_FAIL;
    }

    if (file.gcount() != static_cast<std::streamsize>(total_bytes))
    {
        return Status::E_READ_FAIL;
    }

    return Status::E_OK;
}

/**
 * @brief   Save pixel data from a metadata buffer to a raw binary file.
 *
 * Writes exactly pixel_count * sizeof(PixelT) bytes to the ./assets/
 * directory. Alignment padding is not written — only actual pixel data.
 *
 * @tparam  PixelT      Pixel component type (default: uint8_t for grayscale).
 * @param   file_name   Name of the file to create or overwrite.
 * @param   metadata    Metadata struct containing the image buffer and dimensions.
 * @return  Status::E_OK on success, or an error code on failure.
 */
template <typename PixelT = uint8_t>
[[nodiscard]] Status save_raw(std::string_view file_name,
                              const metadata_t<PixelT>& metadata)
{
    if (!metadata.buffer)
    {
        return Status::E_INVAL_PTR;
    }

    if (!metadata.pixel_count)
    {
        return Status::E_INVAL_SIZE;
    }

    std::filesystem::path file_path = std::filesystem::path("./assets/") / file_name;
    std::ofstream file(file_path, std::ios::binary);

    if (!file)
    {
        return Status::E_INVAL_DIR;
    }

    const size_t total_bytes = metadata.pixel_count * sizeof(PixelT);

    if (!file.write(reinterpret_cast<const char*>(metadata.buffer.get()), total_bytes))
    {
        return Status::E_WRITE_FAIL;
    }

    return Status::E_OK;
}

} // namespace image::io
