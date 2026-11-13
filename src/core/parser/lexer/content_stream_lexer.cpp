#include "ripper/pdf/core/parser/lexer/content_stream_lexer.hpp"

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/byte.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] constexpr bool is_whitespace(char c) noexcept
{
    switch (c)
    {
        case '\0':
        case '\t':
        case '\n':
        case '\f':
        case '\r':
        case ' ':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool is_delimiter(char c) noexcept
{
    switch (c)
    {
        case '(':
        case ')':
        case '<':
        case '>':
        case '[':
        case ']':
        case '{':
        case '}':
        case '/':
        case '%':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool is_number_start(char c) noexcept
{
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

[[nodiscard]] constexpr bool is_invalid_number_lexeme(std::string_view lexeme) noexcept
{
    return lexeme == "+" || lexeme == "-" || lexeme == "." || lexeme == "+." || lexeme == "-.";
}

/// Decode the raw contents of a hex string (`<...>`) into bytes.
///
/// Whitespace between hex digits is ignored. A trailing lone nibble is padded
/// with a low zero per ISO 32000-1 §7.3.4.3.
[[nodiscard]] std::vector<std::byte> decode_hex_string(std::string_view lexeme)
{
    constexpr auto hex_value = [](unsigned char c) noexcept -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    std::vector<std::byte> decoded;
    decoded.reserve(lexeme.size() / 2 + 1);

    int high = -1;
    for (const char raw : lexeme)
    {
        const auto c = static_cast<unsigned char>(raw);
        if (std::isspace(c) != 0)
            continue;

        const int value = hex_value(c);
        if (value < 0)
            throw parse_exception{"Invalid hex digit in hex string"};

        if (high < 0)
        {
            high = value;
        }
        else
        {
            decoded.push_back(
                static_cast<std::byte>(static_cast<unsigned char>((high << 4) | value)));
            high = -1;
        }
    }

    if (high >= 0)
        decoded.push_back(static_cast<std::byte>(static_cast<unsigned char>(high << 4)));

    return decoded;
}

/// Locate the `EI` that terminates inline-image data.
///
/// Per §8.9.7 the marker is preceded by a single white-space character and
/// followed by white space, a delimiter, or end-of-buffer. Binary image data may
/// itself contain the bytes `E` `I`, so this is inherently a heuristic; the
/// authoritative escape hatch is the `/L` entry handled by the caller.
[[nodiscard]] std::size_t find_inline_image_end(std::string_view content, std::size_t from) noexcept
{
    const auto limit = content.size();
    for (auto position = from; position + 1 < limit; ++position)
    {
        if (content[position] != 'E' || content[position + 1] != 'I')
            continue;

        const bool separated_before = position > 0 && is_whitespace(content[position - 1]);
        const bool separated_after =
            position + 2 >= limit || text::is_token_boundary(content[position + 2]);

        if (separated_before && separated_after)
            return position;
    }

    return std::string_view::npos;
}
} // namespace

content_stream_lexer::content_stream_lexer(std::span<const std::byte> data, std::size_t max_depth)
    : content_{byte::as_chars(data.data()), data.size()}, max_depth_{max_depth}
{
}

bool content_stream_lexer::at_end() const noexcept
{
    return position_ >= content_.size();
}

void content_stream_lexer::skip_whitespace_and_comments()
{
    while (!at_end())
    {
        const char c = content_[position_];

        if (is_whitespace(c))
        {
            ++position_;
            continue;
        }

        if (c == '%')
        {
            ++position_;
            while (!at_end() && content_[position_] != '\n' && content_[position_] != '\r')
                ++position_;
            continue;
        }

        break;
    }
}

std::string_view content_stream_lexer::read_keyword()
{
    const auto start = position_;
    while (!at_end())
    {
        const char c = content_[position_];
        if (is_whitespace(c) || is_delimiter(c))
            break;

        ++position_;
    }

    return content_.substr(start, position_ - start);
}

object content_stream_lexer::parse_number()
{
    const auto start = position_;

    if (!at_end() && (content_[position_] == '+' || content_[position_] == '-'))
        ++position_;

    bool saw_digit = false;
    bool saw_dot = false;

    while (!at_end())
    {
        const char c = content_[position_];

        if (std::isdigit(static_cast<unsigned char>(c)) != 0)
        {
            saw_digit = true;
            ++position_;
            continue;
        }

        if (c == '.' && !saw_dot)
        {
            saw_dot = true;
            ++position_;
            continue;
        }

        break;
    }

    const auto lexeme = content_.substr(start, position_ - start);
    if (!saw_digit || is_invalid_number_lexeme(lexeme))
        throw parse_exception{"Invalid numeric token lexeme: " + std::string{lexeme}};

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (saw_dot)
    {
        double real = 0.0;
        auto [ptr, ec] = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), real);
        if (ec == std::errc{})
            return object{real};

        return object{std::string{lexeme}};
    }

    std::int64_t integer = 0;
    auto [ptr, ec] = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), integer);
    if (ec == std::errc{})
        return object{integer};

    return object{std::string{lexeme}};
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

object content_stream_lexer::parse_name()
{
    ++position_; // consume '/'

    const auto start = position_;
    while (!at_end() && !is_whitespace(content_[position_]) && !is_delimiter(content_[position_]))
    {
        ++position_;
    }

    return object{name_object{text::unescape_name(content_.substr(start, position_ - start))}};
}

object content_stream_lexer::parse_literal_string()
{
    ++position_; // consume '('

    const auto start = position_;
    std::size_t depth = 1;
    bool escaped = false;

    while (!at_end())
    {
        const char current = content_[position_];
        ++position_;

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (current == '\\')
        {
            escaped = true;
            continue;
        }

        if (current == '(')
        {
            ++depth;
            continue;
        }

        if (current == ')')
        {
            --depth;
            if (depth == 0)
            {
                return object{string_object{
                    text::unescape_literal_string(content_.substr(start, position_ - 1 - start))}};
            }
        }
    }

    throw parse_exception{"Unterminated literal string in content stream"};
}

object content_stream_lexer::parse_hex_string()
{
    ++position_; // consume '<'

    const auto start = position_;
    while (!at_end() && content_[position_] != '>')
        ++position_;

    if (at_end())
        throw parse_exception{"Unterminated hex string in content stream"};

    const auto lexeme = content_.substr(start, position_ - start);
    ++position_; // consume '>'

    std::vector<std::byte> original;
    original.reserve(lexeme.size());
    for (const char c : lexeme)
        original.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));

    return object{string_object{decode_hex_string(lexeme), std::move(original),
                                string_object::encoding::utf8, string_object::form::hex}};
}

array_object content_stream_lexer::parse_array(std::size_t depth)
{
    if (depth > max_depth_)
        throw parse_exception{"Maximum content operand nesting depth exceeded"};

    ++position_; // consume '['

    array_object array;
    while (true)
    {
        skip_whitespace_and_comments();

        if (at_end())
            throw parse_exception{"Unexpected end of content stream inside array"};

        if (content_[position_] == ']')
        {
            ++position_;
            break;
        }

        array.push_back(parse_operand(depth));
    }

    return array;
}

dictionary_object content_stream_lexer::parse_dictionary(std::size_t depth)
{
    if (depth > max_depth_)
        throw parse_exception{"Maximum content operand nesting depth exceeded"};

    position_ += 2; // consume '<<'

    dictionary_object dictionary;
    while (true)
    {
        skip_whitespace_and_comments();

        if (at_end())
            throw parse_exception{"Unexpected end of content stream inside dictionary"};

        if (content_.substr(position_).starts_with(">>"))
        {
            position_ += 2;
            break;
        }

        if (content_[position_] != '/')
            throw parse_exception{"Content stream dictionary keys must be names"};

        const auto key = parse_name();
        auto value = parse_operand(depth);
        dictionary.set(std::move(key.as_name()->value), std::move(value));
    }

    return dictionary;
}

object content_stream_lexer::parse_operand(std::size_t depth)
{
    if (depth > max_depth_)
        throw parse_exception{"Maximum content operand nesting depth exceeded"};

    skip_whitespace_and_comments();

    if (at_end())
        throw parse_exception{"Unexpected end of content stream while parsing an operand"};

    const char c = content_[position_];

    if (c == '/')
        return parse_name();

    if (c == '(')
        return parse_literal_string();

    if (c == '[')
        return object{parse_array(depth + 1)};

    if (c == '<')
    {
        if (position_ + 1 < content_.size() && content_[position_ + 1] == '<')
            return object{parse_dictionary(depth + 1)};

        return parse_hex_string();
    }

    if (is_number_start(c))
        return parse_number();

    const auto keyword = read_keyword();

    if (keyword == "true")
        return object{boolean_object{true}};

    if (keyword == "false")
        return object{boolean_object{false}};

    if (keyword == "null")
        return object{};

    throw parse_exception{"Unexpected token while parsing a content stream operand: " +
                          std::string{keyword}};
}

content_stream_token content_stream_lexer::next()
{
    skip_whitespace_and_comments();

    if (at_end())
        return content_stream_token{};

    const char c = content_[position_];

    if (c == '/' || c == '(' || c == '[' || c == '<' || is_number_start(c))
    {
        return content_stream_token{.kind = content_stream_token_kind::operand,
                                    .operand = parse_operand(0)};
    }

    const auto keyword = read_keyword();
    if (keyword.empty())
        throw parse_exception{"Unexpected character in content stream: " + std::string(1, c)};

    if (keyword == "BI")
    {
        return content_stream_token{.kind = content_stream_token_kind::inline_image,
                                    .image = scan_inline_image()};
    }

    if (keyword == "true")
        return content_stream_token{.kind = content_stream_token_kind::operand,
                                    .operand = object{boolean_object{true}}};

    if (keyword == "false")
        return content_stream_token{.kind = content_stream_token_kind::operand,
                                    .operand = object{boolean_object{false}}};

    if (keyword == "null")
        return content_stream_token{.kind = content_stream_token_kind::operand,
                                    .operand = object{}};

    return content_stream_token{.kind = content_stream_token_kind::operator_name,
                                .operator_name = std::string{keyword}};
}

inline_image content_stream_lexer::scan_inline_image()
{
    dictionary_object dictionary;

    // The inline-image dictionary is a bare run of key/value pairs, not a real
    // dictionary object, and it ends at the `ID` keyword.
    while (true)
    {
        skip_whitespace_and_comments();

        if (at_end())
            throw parse_exception{
                "Unexpected end of content stream inside inline image dictionary"};

        if (content_[position_] == '/')
        {
            const auto key = parse_name();
            auto value = parse_operand(0);
            dictionary.set(std::move(key.as_name()->value), std::move(value));
            continue;
        }

        const auto keyword = read_keyword();
        if (keyword == "ID")
            break;

        throw parse_exception{"Unexpected token in inline image dictionary: " +
                              std::string{keyword}};
    }

    // Data begins after exactly one white-space character following `ID`.
    std::size_t data_start = position_;
    if (data_start < content_.size() && is_whitespace(content_[data_start]))
        ++data_start;

    std::size_t data_end = std::string_view::npos;
    std::size_t resume = std::string_view::npos;

    // `/L` (Table 93) is the authoritative data length when present. Honour it
    // before falling back to scanning, so data containing `EI` stays intact.
    const auto* declared_length = dictionary.get_number("L");
    if (declared_length != nullptr && declared_length->is_integer() &&
        declared_length->as_integer() >= 0)
    {
        const auto declared = static_cast<std::size_t>(declared_length->as_integer());
        if (declared <= content_.size() - data_start)
        {
            auto marker = data_start + declared;
            while (marker < content_.size() && is_whitespace(content_[marker]))
                ++marker;

            if (marker + 2 <= content_.size() && content_[marker] == 'E' &&
                content_[marker + 1] == 'I')
            {
                data_end = data_start + declared;
                resume = marker + 2;
            }
        }
    }

    if (data_end == std::string_view::npos)
    {
        const auto marker = find_inline_image_end(content_, data_start);
        if (marker == std::string_view::npos)
            throw parse_exception{"Unterminated inline image: missing EI marker"};

        // The single white-space character before `EI` is the separator and is
        // not part of the image data.
        data_end = std::max(data_start, marker - 1);
        resume = marker + 2;
    }

    std::vector<std::byte> data(data_end - data_start);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::memcpy(data.data(), content_.data() + data_start, data.size());

    position_ = resume;
    return inline_image{std::move(dictionary), std::move(data)};
}
} // namespace ripper::pdf::core
