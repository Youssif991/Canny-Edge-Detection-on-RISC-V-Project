/**
 * @file image_io.cpp
 * @brief Raw grayscale image load and save — standard C++ file streams.
 * @author Youssef
 */

#include "image_io.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>

uint8_t* alloc_image(int width, int height) {
    std::size_t size = static_cast<std::size_t>(width) * height;
    return static_cast<uint8_t*>(aligned_alloc(64, size));
}

bool load_image(const char* filepath, uint8_t* buffer, int width, int height) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[load_image] Could not open: " << filepath << "\n";
        return false;
    }
    std::size_t size = static_cast<std::size_t>(width) * height;
    file.read(reinterpret_cast<char*>(buffer), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) {
        std::cerr << "[load_image] Expected " << size
                  << " bytes, got " << file.gcount() << "\n";
        return false;
    }
    return true;
}

bool save_image(const char* filepath, const uint8_t* buffer, int width, int height) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[save_image] Could not open: " << filepath << "\n";
        return false;
    }
    std::size_t size = static_cast<std::size_t>(width) * height;
    file.write(reinterpret_cast<const char*>(buffer), size);
    if (!file.good()) {
        std::cerr << "[save_image] Write failed for: " << filepath << "\n";
        return false;
    }
    return true;
}