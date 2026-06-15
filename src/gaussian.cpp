/**
 * @file    gaussian.cpp
 * @brief   Gaussian blur implementations for image processing.
 */

#include "gaussian.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace processing
{

namespace
{

/**
 * @brief   Helper to allocate a 64-byte aligned buffer and pad the input image with zero-padding.
 *
 * @tparam  PixelT         Pixel component type.
 * @param   input_image    Reference to the source image metadata.
 * @param   kernel_radius  The radius of the convolution kernel (padding thickness).
 * @param   padded_image   Reference to a unique_ptr to hold the allocated and padded buffer.
 * @param   padded_width   Reference to receive the computed width of the padded image.
 * @param   padded_height  Reference to receive the computed height of the padded image.
 * @return  Status         E_OK on success, or a Status error code on failure.
 */
template <typename PixelT>
Status allocate_and_pad_image(
    const image::io::metadata_t<PixelT>& input_image,
    int32_t kernel_radius,
    std::unique_ptr<PixelT[], utils::memory::deleter>& padded_image,
    uint32_t& padded_width,
    uint32_t& padded_height)
{
    const int32_t image_width  = static_cast<int32_t>(input_image.width);
    const int32_t image_height = static_cast<int32_t>(input_image.height);

    padded_width  = image_width  + 2 * kernel_radius;
    padded_height = image_height + 2 * kernel_radius;
    const uint32_t padded_size = padded_width * padded_height;

    auto raw_ptr = static_cast<PixelT*>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(padded_size * sizeof(PixelT))));
    if (!raw_ptr)
    {
        return Status::E_ALLOC_FAIL;
    }

    padded_image.reset(raw_ptr);
    std::fill(padded_image.get(), padded_image.get() + padded_size, PixelT{0});

    for (int32_t r = 0; r < image_height; ++r)
    {
        std::copy_n(
            &input_image.buffer.get()[r * image_width],
            image_width,
            &padded_image.get()[(r + kernel_radius) * padded_width + kernel_radius]);
    }

    return Status::E_OK;
}

} // namespace

/**
 * @brief   Apply a 5x5 Gaussian blur using a full 2D convolution.
 *
 * The input image is zero-padded, accumulated row by row, and then
 * normalized back into the destination buffer.
 *
 * @tparam  PixelT       Pixel component type.
 * @tparam  AccumT       Accumulator type used for intermediate sums.
 * @param   input_image  Image metadata containing the input buffer and dimensions.
 * @return  Status code indicating success or the failure reason.
 */
template <typename PixelT, typename AccumT>
Status gaussian_spatial_5x5(image::io::metadata_t<PixelT>& input_image)
{
    if (!input_image.height || !input_image.width || !input_image.buffer)
    {
        return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
    }

    const int32_t image_width   = static_cast<int32_t>(input_image.width);
    const int32_t image_height  = static_cast<int32_t>(input_image.height);
    const int32_t kernel_radius = 2;

    uint32_t padded_width = 0;
    uint32_t padded_height = 0;
    std::unique_ptr<PixelT[], utils::memory::deleter> padded_image;

    Status status = allocate_and_pad_image(input_image, kernel_radius, padded_image, padded_width, padded_height);
    if (status != Status::E_OK)
    {
        return status;
    }

    auto output_image_raw = static_cast<PixelT*>(
        utils::memory::aligned_alloc(64, input_image.aligned_buffer_size));
    if (!output_image_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<PixelT[], utils::memory::deleter> output_image(output_image_raw);

    auto row_accumulator_raw = static_cast<AccumT*>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(image_width * sizeof(AccumT))));
    if (!row_accumulator_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<AccumT[], utils::memory::deleter> row_accumulator(row_accumulator_raw);

    constexpr uint32_t shift      = 16;
    constexpr uint64_t multiplier = (1ULL << shift) / 273;

    // Use restrict pointers for optimization
    const PixelT* __restrict padded_ptr = padded_image.get();
    PixelT* __restrict out_ptr = output_image.get();
    AccumT* __restrict acc_ptr = row_accumulator.get();

    for (int32_t row_index = 0; row_index < image_height; ++row_index)
    {
        std::fill(acc_ptr, acc_ptr + image_width, AccumT{0});

        for (int32_t kernel_row_offset = -kernel_radius; kernel_row_offset <= kernel_radius; ++kernel_row_offset)
        {
            const uint32_t kernel_row_index = static_cast<uint32_t>(kernel_row_offset + kernel_radius);
            const uint32_t padded_row_index = static_cast<uint32_t>(row_index + kernel_radius + kernel_row_offset);

            for (int32_t kernel_col_offset = -kernel_radius; kernel_col_offset <= kernel_radius; ++kernel_col_offset)
            {
                const AccumT kernel_weight = static_cast<AccumT>(
                    kernels::GAUSSIAN_5X5[kernel_row_index][kernel_col_offset + kernel_radius]);
                const uint32_t padded_col_offset = static_cast<uint32_t>(kernel_radius + kernel_col_offset);

                for (int32_t col_index = 0; col_index < image_width; ++col_index)
                {
                    acc_ptr[col_index] += kernel_weight *
                        static_cast<AccumT>(
                            padded_ptr[padded_row_index * padded_width + col_index + padded_col_offset]);
                }
            }
        }

        for (int32_t col_index = 0; col_index < image_width; ++col_index)
        {
            AccumT pixel_value = static_cast<AccumT>(
                (static_cast<uint64_t>(acc_ptr[col_index]) * multiplier) >> shift);
            out_ptr[row_index * image_width + col_index] = static_cast<PixelT>(
                pixel_value < 0 ? 0 : pixel_value > 255 ? 255 : pixel_value);
        }
    }

    input_image.buffer = std::move(output_image);

    return Status::E_OK;
}

/**
 * @brief   Apply a 5x5 Gaussian blur using separable 1x5 horizontal and vertical passes.
 *
 * The input image is zero-padded once, processed through a horizontal pass into
 * an intermediate buffer, and then processed through a vertical pass into the
 * output buffer.
 *
 * @tparam  PixelT       Pixel component type.
 * @tparam  AccumT       Accumulator type used for intermediate sums.
 * @param   input_image  Image metadata containing the input buffer and dimensions.
 * @return  Status code indicating success or the failure reason.
 */
template <typename PixelT, typename AccumT>
Status gaussian_separable_5x5(image::io::metadata_t<PixelT>& input_image)
{
    if (!input_image.height || !input_image.width || !input_image.buffer)
    {
        return input_image.buffer ? Status::E_NOK : Status::E_INVAL_PTR;
    }

    const int32_t image_width   = static_cast<int32_t>(input_image.width);
    const int32_t image_height  = static_cast<int32_t>(input_image.height);
    const int32_t kernel_radius = 2;

    uint32_t padded_width = 0;
    uint32_t padded_height = 0;
    std::unique_ptr<PixelT[], utils::memory::deleter> padded_image;

    Status status = allocate_and_pad_image(input_image, kernel_radius, padded_image, padded_width, padded_height);
    if (status != Status::E_OK)
    {
        return status;
    }

    const uint32_t padded_image_size = padded_width * padded_height;
    auto horizontal_image_raw = static_cast<AccumT*>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(padded_image_size * sizeof(AccumT))));
    if (!horizontal_image_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<AccumT[], utils::memory::deleter> horizontal_image(horizontal_image_raw);
    std::fill(horizontal_image.get(), horizontal_image.get() + padded_image_size, AccumT{0});

    auto output_image_raw = static_cast<PixelT*>(
        utils::memory::aligned_alloc(64, input_image.aligned_buffer_size));
    if (!output_image_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<PixelT[], utils::memory::deleter> output_image(output_image_raw);

    auto row_accumulator_raw = static_cast<AccumT*>(
        utils::memory::aligned_alloc(64,
            utils::memory::align_64(image_width * sizeof(AccumT))));
    if (!row_accumulator_raw)
    {
        return Status::E_ALLOC_FAIL;
    }
    std::unique_ptr<AccumT[], utils::memory::deleter> row_accumulator(row_accumulator_raw);

    constexpr uint32_t shift      = 16;
    constexpr uint64_t multiplier = (1ULL << shift) / (17 * 17);

    // Use restrict pointers for optimization
    const PixelT* __restrict padded_ptr = padded_image.get();
    AccumT* __restrict horiz_ptr = horizontal_image.get();
    PixelT* __restrict out_ptr = output_image.get();
    AccumT* __restrict acc_ptr = row_accumulator.get();

    for (int32_t row_index = 0; row_index < image_height; ++row_index)
    {
        const uint32_t padded_row_index = static_cast<uint32_t>(row_index + kernel_radius);

        for (int32_t kernel_col_offset = -kernel_radius; kernel_col_offset <= kernel_radius; ++kernel_col_offset)
        {
            const AccumT kernel_weight = static_cast<AccumT>(
                kernels::GAUSSIAN_1X5[kernel_col_offset + kernel_radius]);
            const uint32_t padded_col_offset = static_cast<uint32_t>(kernel_radius + kernel_col_offset);

            for (int32_t col_index = 0; col_index < image_width; ++col_index)
            {
                horiz_ptr[padded_row_index * padded_width + kernel_radius + col_index] += kernel_weight *
                    static_cast<AccumT>(
                        padded_ptr[padded_row_index * padded_width + col_index + padded_col_offset]);
            }
        }
    }

    for (int32_t row_index = 0; row_index < image_height; ++row_index)
    {
        std::fill(acc_ptr, acc_ptr + image_width, AccumT{0});

        for (int32_t kernel_row_offset = -kernel_radius; kernel_row_offset <= kernel_radius; ++kernel_row_offset)
        {
            const AccumT kernel_weight = static_cast<AccumT>(
                kernels::GAUSSIAN_1X5[kernel_row_offset + kernel_radius]);
            const uint32_t padded_row_index = static_cast<uint32_t>(row_index + kernel_radius + kernel_row_offset);

            for (int32_t col_index = 0; col_index < image_width; ++col_index)
            {
                acc_ptr[col_index] += kernel_weight *
                    static_cast<AccumT>(
                        horiz_ptr[padded_row_index * padded_width + kernel_radius + col_index]);
            }
        }

        for (int32_t col_index = 0; col_index < image_width; ++col_index)
        {
            AccumT pixel_value = static_cast<AccumT>(
                (static_cast<uint64_t>(acc_ptr[col_index]) * multiplier) >> shift);
            out_ptr[row_index * image_width + col_index] = static_cast<PixelT>(
                pixel_value < 0 ? 0 : pixel_value > 255 ? 255 : pixel_value);
        }
    }

    input_image.buffer = std::move(output_image);

    return Status::E_OK;
}


template Status gaussian_spatial_5x5<uint8_t,  int32_t>(image::io::metadata_t<uint8_t>&);
template Status gaussian_separable_5x5<uint8_t, int32_t>(image::io::metadata_t<uint8_t>&);

} // namespace processing