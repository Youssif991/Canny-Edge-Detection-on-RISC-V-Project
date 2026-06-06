/**
 * @file image_io.hpp
 * @brief Raw grayscale image load, save, and buffer allocation utilities.
 *        Format: width * height bytes, one byte per pixel, no header.
 * @author Youssef
 */

#pragma once
#include <cstdint>
#include <cstddef>

/**
 * @brief Allocate a 64-byte aligned image buffer.
 *        Required for RVV vector load intrinsics in Phase 6.
 *        Caller must free() the returned pointer.
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 * @return 64-byte aligned buffer of width * height bytes, nullptr on failure.
 */
uint8_t* alloc_image(int width, int height);

/**
 * @brief Load a raw grayscale image from disk into a pre-allocated buffer.
 * @param filepath  Path to the raw image file.
 * @param buffer    64-byte aligned buffer of size width * height bytes.
 * @param width     Image width in pixels.
 * @param height    Image height in pixels.
 * @return true if file opened and exactly width * height bytes were read.
 */
bool load_image(const char* filepath, uint8_t* buffer, int width, int height);

/**
 * @brief Save a raw grayscale image buffer to disk.
 * @param filepath  Path to the output raw file.
 * @param buffer    Buffer containing width * height pixel bytes.
 * @param width     Image width in pixels.
 * @param height    Image height in pixels.
 * @return true if file opened and all bytes written successfully.
 */
bool save_image(const char* filepath, const uint8_t* buffer, int width, int height);