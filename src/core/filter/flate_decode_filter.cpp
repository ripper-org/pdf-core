#include "ripper/pdf/core/filter/flate_decode_filter.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <limits>
#include <zlib.h>

namespace ripper::pdf::core
{
namespace
{

[[nodiscard]] Bytef* as_zlib_buffer(std::byte* p) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<Bytef*>(p);
}

[[nodiscard]] const Bytef* as_zlib_buffer(const std::byte* p) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const Bytef*>(p);
}

/// zlib's inflate API does not modify its input, but historically takes a
/// non-const pointer. Centralize the required cast here.
[[nodiscard]] Bytef* as_zlib_input(const std::byte* p) noexcept
{
    const auto* bytes = as_zlib_buffer(p);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return const_cast<Bytef*>(bytes);
}

} // namespace

std::vector<std::byte> flate_decode_filter::decode(std::span<const std::byte> input,
                                                   const dictionary_object* params) const
{
    return decode_with_limits(input, params, {});
}

std::vector<std::byte> flate_decode_filter::decode_with_limits(std::span<const std::byte> input,
                                                               const dictionary_object* /*params*/,
                                                               const resource_limits& limits) const
{
    if (input.empty())
    {
        return {};
    }

    const std::size_t max_output = limits.max_decoded_stream_bytes;
    if (max_output == 0)
        throw parse_exception{"Decoded stream size limit is zero"};

    z_stream stream{};
    const int init_result = inflateInit(&stream);
    if (init_result != Z_OK)
        throw io_exception{"Decompression failed to initialize"};

    struct inflate_guard
    {
        explicit inflate_guard(z_stream& state) noexcept : stream{&state} {}

        inflate_guard(const inflate_guard&) = delete;
        inflate_guard& operator=(const inflate_guard&) = delete;
        inflate_guard(inflate_guard&&) = delete;
        inflate_guard& operator=(inflate_guard&&) = delete;

        ~inflate_guard()
        {
            (void)inflateEnd(stream);
        }

        z_stream* stream;
    } guard{stream};

    // Inflate incrementally so output growth is bounded by the configured limit
    // instead of repeatedly allocating larger buffers via uncompress().
    constexpr std::size_t output_chunk_size = std::size_t{64U} * 1024U;
    std::array<std::byte, output_chunk_size> chunk{};

    std::vector<std::byte> output;
    std::size_t input_pos = 0;

    for (;;)
    {
        if (stream.avail_in == 0 && input_pos < input.size())
        {
            const std::size_t remaining = input.size() - input_pos;
            const auto available = static_cast<uInt>(
                std::min(remaining, static_cast<std::size_t>(std::numeric_limits<uInt>::max())));
            stream.next_in = as_zlib_input(input.subspan(input_pos).data());
            stream.avail_in = available;
            input_pos += available;
        }

        stream.next_out = as_zlib_buffer(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        const int result = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = chunk.size() - stream.avail_out;

        if (produced > max_output - output.size())
            throw parse_exception{"Decoded stream exceeds the configured size limit"};

        output.insert(output.end(), chunk.begin(),
                      std::next(chunk.begin(), static_cast<std::ptrdiff_t>(produced)));

        if (result == Z_STREAM_END)
            break;

        if (result == Z_OK)
            continue;

        if (result == Z_BUF_ERROR)
        {
            if (stream.avail_in == 0 && input_pos >= input.size())
                throw parse_exception{"Compressed stream is truncated"};

            continue;
        }

        switch (result)
        {
            case Z_MEM_ERROR:
                throw io_exception{"Decompression failed due to memory error"};
            case Z_DATA_ERROR:
                throw parse_exception{"Compressed stream is corrupted"};
            default:
                throw io_exception{"Decompression failed"};
        }
    }

    return output;
}

std::vector<std::byte> flate_decode_filter::encode(std::span<const std::byte> input,
                                                   const dictionary_object* /*params*/) const
{
    if (input.empty())
    {
        return {};
    }

    const uLongf maxSize = compressBound(static_cast<uLong>(input.size()));
    std::vector<std::byte> output(static_cast<std::size_t>(maxSize));

    uLongf compressedSize = maxSize;
    const int result = ::compress(as_zlib_buffer(output.data()), &compressedSize,
                                  as_zlib_buffer(input.data()), static_cast<uLong>(input.size()));

    if (result != Z_OK)
    {
        switch (result)
        {
            case Z_MEM_ERROR:
                throw io_exception{"Compression failed due to memory error"};
            case Z_BUF_ERROR:
                throw io_exception{"Compression output buffer too small"};
            default:
                throw io_exception{"Compression failed"};
        }
    }

    output.resize(static_cast<std::size_t>(compressedSize));
    return output;
}

} // namespace ripper::pdf::core
