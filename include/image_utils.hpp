/**
 * @file    image_utils.hpp
 * @brief   Shared utility functions for memory alignment and status reporting.
 * @author  Youssef
 */

#pragma once
#include "image_types.hpp"
#include <cstddef>
#include <cstdio>

namespace utils
{

// -----------------------------------------------------------------------------
// Memory utilities
// -----------------------------------------------------------------------------
namespace memory
{

/**
 * @brief   Round up a byte count to the nearest 64-byte boundary.
 *          Required so aligned_alloc(64, size) receives a valid size argument —
 *          aligned_alloc requires the size to be a multiple of the alignment.
 * @param   size    Raw byte count to align.
 * @return  Smallest multiple of 64 that is >= size.
 */
[[nodiscard]] inline size_t align_64(size_t size)
{
    return (size + 63u) & ~63u;
}

} // namespace memory

// -----------------------------------------------------------------------------
// Status utilities
// -----------------------------------------------------------------------------
namespace status
{

/**
 * @brief   Convert a Status code to a human-readable string.
 * @param   s   Status code to convert.
 * @return  Null-terminated string describing the status.
 */
[[nodiscard]] inline const char* to_string(Status s)
{
    switch (s)
    {
        case Status::E_OK:         return "E_OK";
        case Status::E_NOK:        return "E_NOK";
        case Status::E_ALLOC_FAIL: return "E_ALLOC_FAIL";
        case Status::E_INVAL_PTR:  return "E_INVAL_PTR";
        case Status::E_INVAL_DIR:  return "E_INVAL_DIR";
        case Status::E_INVAL_SIZE: return "E_INVAL_SIZE";
        case Status::E_READ_FAIL:  return "E_READ_FAIL";
        case Status::E_WRITE_FAIL: return "E_WRITE_FAIL";
        default:                   return "UNKNOWN";
    }
}

/**
 * @brief   Print a status code with a context message to stderr.
 * @param   context     Short description of where the error occurred.
 * @param   s           Status code to report.
 */
inline void report(const char* context, Status s)
{
    if (s != Status::E_OK)
    {
        std::fprintf(stderr, "[ERROR] %s: %s\n", context, to_string(s));
    }
}

} // namespace status

} // namespace utils
