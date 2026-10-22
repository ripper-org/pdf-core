#include "object_stream_layout.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/byte.hpp"
#include "ripper/pdf/core/util/checked.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

namespace ripper::pdf::core
{
namespace
{

[[nodiscard]] std::size_t skip_whitespace(std::span<const std::byte> content, std::size_t pos,
                                          std::size_t limit) noexcept
{
    limit = std::min(limit, content.size());
    while (pos < limit && byte::is_whitespace(content[pos]))
        ++pos;

    return pos;
}

struct token_range
{
    std::size_t begin;
    std::size_t end;
};

/// Read one whitespace-delimited token from `[pos, limit)`.
[[nodiscard]] token_range read_token(std::span<const std::byte> content, std::size_t pos,
                                     std::size_t limit)
{
    limit = std::min(limit, content.size());
    pos = skip_whitespace(content, pos, limit);
    const auto begin = pos;

    while (pos < limit && !byte::is_whitespace(content[pos]))
        ++pos;

    if (pos == begin)
        throw parse_exception{"Object stream header has a missing or truncated entry"};

    return {begin, pos};
}

template <typename T>
[[nodiscard]] T parse_token(std::span<const std::byte> content, token_range token,
                            std::string_view what)
{
    const auto token_bytes = content.subspan(token.begin, token.end - token.begin);
    const auto* first = byte::as_chars(token_bytes.data());
    const auto* last = std::next(first, static_cast<std::ptrdiff_t>(token_bytes.size()));

    T value{};
    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last)
        throw parse_exception{std::string{what} + " is invalid in object stream header"};

    return value;
}

} // namespace

std::uint32_t object_stream_count(const dictionary_object& dict, const resource_limits& limits)
{
    const auto* n = dict.get_number("N");
    if (n == nullptr || !n->is_integer() || n->as_integer() <= 0)
        throw parse_exception{"Object stream missing required positive integer /N"};

    const auto count = checked::to_unsigned<std::uint32_t>(n->as_integer(), "Object stream /N");

    if (count > limits.max_object_stream_objects)
        throw parse_exception{"Object stream /N exceeds the configured object limit"};

    return count;
}

std::size_t object_stream_first(const dictionary_object& dict, std::size_t content_size)
{
    const auto* first = dict.get_number("First");
    if (first == nullptr || !first->is_integer() || first->as_integer() < 0)
        throw parse_exception{"Object stream missing required non-negative integer /First"};

    const auto value =
        checked::to_unsigned<std::size_t>(first->as_integer(), "Object stream /First");

    if (value > content_size)
        throw parse_exception{"Object stream /First is beyond the end of the stream"};

    return value;
}

object_stream_layout parse_object_stream_layout(std::span<const std::byte> content,
                                                const dictionary_object& dict,
                                                const resource_limits& limits)
{
    object_stream_layout layout;
    layout.count = object_stream_count(dict, limits);
    layout.first = object_stream_first(dict, content.size());

    // Reserve proportional to the physically available header, not to the
    // untrusted /N value.
    layout.entries.reserve(std::min<std::size_t>(layout.count, layout.first));

    std::size_t pos = 0;
    for (std::uint32_t i = 0; i < layout.count; ++i)
    {
        const auto object_number_token = read_token(content, pos, layout.first);
        pos = object_number_token.end;

        const auto offset_token = read_token(content, pos, layout.first);
        pos = offset_token.end;

        const auto object_number =
            parse_token<std::uint32_t>(content, object_number_token, "Object number");
        const auto relative_offset =
            parse_token<std::size_t>(content, offset_token, "Object byte offset");

        if (relative_offset > content.size() - layout.first)
            throw parse_exception{"Object stream byte offset is out of bounds"};

        layout.entries.push_back({object_number, layout.first + relative_offset, 0});
    }

    for (std::size_t i = 0; i < layout.entries.size(); ++i)
    {
        auto& entry = layout.entries[i];
        const auto end =
            (i + 1 < layout.entries.size()) ? layout.entries[i + 1].offset : content.size();

        if (entry.offset > end || end > content.size())
            throw parse_exception{"Object stream entry has an invalid or descending byte range"};

        if (entry.offset == end)
            throw parse_exception{"Object stream entry has an empty byte range"};

        entry.length = end - entry.offset;
    }

    return layout;
}

} // namespace ripper::pdf::core
