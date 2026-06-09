/**
 * @file    utils.hpp
 * @brief   Utility functions for memory management and alignment.
 */

#pragma once

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace utils::memory
{

/**
 * @brief   Struct-like function object for aligned memory deallocation.
 *          Used as custom deleter for std::unique_ptr.
 */
struct deleter
{
    void operator()(void* ptr) const noexcept
    {
        std::free(ptr);
    }
};

/**
 * @brief   Allocates memory with a specified alignment.
 * @param   alignment       The alignment boundary (must be a power of 2).
 * @param   size            The number of bytes to allocate.
 * @return                  Pointer to the allocated memory, or nullptr on failure.
 */
inline void* aligned_alloc(size_t alignment, size_t size) noexcept
{
    size_t remainder = size % alignment;
    if (remainder != 0) {
        size += (alignment - remainder);
    }
    return std::aligned_alloc(alignment, size);
}

/**
 * @brief   Generic alignment function.
 * @param   value           The value to align.
 * @param   alignment       The alignment boundary (must be a power of 2).
 * @return                  Smallest multiple of alignment that is >= value.
 */
template <std::integral T>
[[nodiscard]] constexpr T align(T value, size_t alignment) noexcept
{
    const T mask = static_cast<T>(alignment - 1);
    return (value + mask) & ~mask;
}

/**
 * @brief   Round up a byte count to the nearest 64-byte boundary.
 *          Required so aligned_alloc(64, size) receives a valid size argument —
 *          aligned_alloc requires the size to be a multiple of the alignment.
 * @param   value    Raw byte count or pixel count to align.
 * @return  Smallest multiple of 64 that is >= value.
 */
template <std::integral T>
[[nodiscard]] constexpr T align_64(T value) noexcept
{
    return align(value, 64);
}

} // namespace utils::memory
