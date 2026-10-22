#include "ripper/pdf/core/filter/predictor.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/checked.hpp"

#include <algorithm>
#include <cstdint>

namespace ripper::pdf::core
{
namespace
{

[[nodiscard]] std::uint64_t parameter(const dictionary_object& params, const std::string& key,
                                      std::uint64_t fallback)
{
    const auto* number = params.get_number(key);
    if (number == nullptr)
        return fallback;

    const auto value = number->as_integer();
    if (value < 0)
        throw parse_exception{"Predictor parameter /" + key + " must not be negative"};

    return static_cast<std::uint64_t>(value);
}

[[nodiscard]] std::size_t paeth_predictor(std::size_t left, std::size_t up,
                                          std::size_t upper_left) noexcept
{
    const auto p = left + up - upper_left;
    const auto pa = std::abs(static_cast<std::int64_t>(p) - static_cast<std::int64_t>(left));
    const auto pb = std::abs(static_cast<std::int64_t>(p) - static_cast<std::int64_t>(up));
    const auto pc = std::abs(static_cast<std::int64_t>(p) - static_cast<std::int64_t>(upper_left));

    if (pa <= pb && pa <= pc)
        return left;
    if (pb <= pc)
        return up;
    return upper_left;
}

[[nodiscard]] unsigned char add_bytes(unsigned char current, unsigned int delta) noexcept
{
    return static_cast<unsigned char>((static_cast<unsigned int>(current) + delta) & 0xFFU);
}

[[nodiscard]] std::vector<std::byte> apply_png_predictor(std::span<const std::byte> input,
                                                         std::size_t columns, std::size_t colors,
                                                         std::size_t bits_per_component,
                                                         const resource_limits& limits)
{
    const std::size_t samples_per_pixel =
        checked::multiply(colors, bits_per_component, "PNG predictor");
    const std::size_t bytes_per_pixel =
        std::max<std::size_t>(1, checked::add(samples_per_pixel, 7, "PNG predictor") / 8);

    const std::size_t row_bits = checked::multiply(
        checked::multiply(columns, colors, "PNG predictor"), bits_per_component, "PNG predictor");
    const std::size_t row_length = checked::add(row_bits, 7, "PNG predictor") / 8;

    if (row_length == 0 || bytes_per_pixel == 0)
        throw parse_exception{"Invalid PNG predictor parameters"};

    if (row_length > limits.max_predictor_row_bytes)
        throw parse_exception{"PNG predictor row exceeds the configured size limit"};

    if (input.size() > limits.max_decoded_stream_bytes)
        throw parse_exception{"Decoded stream exceeds the configured size limit"};

    const std::size_t stride = checked::add(row_length, 1, "PNG predictor");
    if (input.size() % stride != 0)
        throw parse_exception{"Truncated PNG predictor row"};

    std::vector<std::byte> output;
    output.reserve(input.size());

    std::vector<std::byte> current(row_length, std::byte{0});
    std::vector<std::byte> previous(row_length, std::byte{0});

    std::size_t in_pos = 0;
    const std::size_t input_size = input.size();

    while (in_pos < input_size)
    {
        const auto tag = std::to_integer<unsigned char>(input[in_pos]);
        ++in_pos;

        for (std::size_t i = 0; i < row_length; ++i)
            current[i] = input[in_pos + i];
        in_pos += row_length;

        switch (tag)
        {
            case 0: // None
                break;
            case 1: // Sub
                for (std::size_t i = bytes_per_pixel; i < row_length; ++i)
                    current[i] = static_cast<std::byte>(
                        add_bytes(std::to_integer<unsigned char>(current[i]),
                                  std::to_integer<unsigned char>(current[i - bytes_per_pixel])));
                break;
            case 2: // Up
                for (std::size_t i = 0; i < row_length; ++i)
                    current[i] = static_cast<std::byte>(
                        add_bytes(std::to_integer<unsigned char>(current[i]),
                                  std::to_integer<unsigned char>(previous[i])));
                break;
            case 3: // Average
                for (std::size_t i = 0; i < row_length; ++i)
                {
                    const auto left =
                        i >= bytes_per_pixel
                            ? std::to_integer<unsigned char>(current[i - bytes_per_pixel])
                            : 0;
                    const auto up = std::to_integer<unsigned char>(previous[i]);
                    current[i] = static_cast<std::byte>(
                        add_bytes(std::to_integer<unsigned char>(current[i]),
                                  (static_cast<unsigned int>(left) + up) / 2U));
                }
                break;
            case 4: // Paeth
                for (std::size_t i = 0; i < row_length; ++i)
                {
                    const auto left =
                        i >= bytes_per_pixel
                            ? std::to_integer<unsigned char>(current[i - bytes_per_pixel])
                            : 0;
                    const auto up = std::to_integer<unsigned char>(previous[i]);
                    const auto upper_left =
                        i >= bytes_per_pixel
                            ? std::to_integer<unsigned char>(previous[i - bytes_per_pixel])
                            : 0;
                    current[i] =
                        static_cast<std::byte>(add_bytes(std::to_integer<unsigned char>(current[i]),
                                                         paeth_predictor(left, up, upper_left)));
                }
                break;
            default:
                throw parse_exception{"Unknown PNG predictor filter type"};
        }

        output.insert(output.end(), current.begin(), current.end());
        previous.swap(current);
        std::fill(current.begin(), current.end(), std::byte{0});
    }

    return output;
}

[[nodiscard]] std::vector<std::byte> apply_tiff_predictor(std::span<const std::byte> input,
                                                          std::size_t columns, std::size_t colors,
                                                          const resource_limits& limits)
{
    if (colors == 0 || columns == 0)
        throw parse_exception{"Invalid TIFF predictor parameters"};

    const std::size_t row_length = checked::multiply(columns, colors, "TIFF predictor");
    if (row_length == 0)
        throw parse_exception{"Invalid TIFF predictor row length"};

    if (input.size() > limits.max_decoded_stream_bytes)
        throw parse_exception{"Decoded stream exceeds the configured size limit"};

    if (input.size() % row_length != 0)
        throw parse_exception{"TIFF predictor data is not an integral number of rows"};

    std::vector<std::byte> output(input.begin(), input.end());

    for (std::size_t row = 0; row < output.size() / row_length; ++row)
    {
        for (std::size_t i = colors; i < row_length; ++i)
        {
            const std::size_t index = row * row_length + i;
            output[index] = static_cast<std::byte>(
                add_bytes(std::to_integer<unsigned char>(output[index]),
                          std::to_integer<unsigned char>(output[index - colors])));
        }
    }

    return output;
}

} // namespace

bool has_predictor(const dictionary_object* params)
{
    if (params == nullptr)
        return false;

    const auto* predictor = params->get_number("Predictor");
    return predictor != nullptr && predictor->as_integer() > 1;
}

std::vector<std::byte> apply_predictor(std::span<const std::byte> data,
                                       const dictionary_object& params,
                                       const resource_limits& limits)
{
    const auto predictor = parameter(params, "Predictor", 1);
    if (predictor == 1)
        return {data.begin(), data.end()};

    const auto columns = parameter(params, "Columns", 1);
    const auto colors = parameter(params, "Colors", 1);
    const auto bits_per_component = parameter(params, "BitsPerComponent", 8);

    switch (predictor)
    {
        case 2:
        {
            if (bits_per_component != 8)
                throw parse_exception{"TIFF predictor requires 8-bit samples"};
            return apply_tiff_predictor(data, checked::to_unsigned<std::size_t>(columns, "Columns"),
                                        checked::to_unsigned<std::size_t>(colors, "Colors"),
                                        limits);
        }
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            return apply_png_predictor(
                data, checked::to_unsigned<std::size_t>(columns, "Columns"),
                checked::to_unsigned<std::size_t>(colors, "Colors"),
                checked::to_unsigned<std::size_t>(bits_per_component, "BitsPerComponent"), limits);
        default:
            throw parse_exception{"Unsupported stream predictor value"};
    }
}

} // namespace ripper::pdf::core
