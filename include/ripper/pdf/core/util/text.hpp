#pragma once

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ripper::pdf::core::text
{
[[nodiscard]] constexpr std::string_view strip_line_endings(std::string_view s) noexcept
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    {
        s.remove_suffix(1);
    }

    return s;
}

[[nodiscard]] inline std::string_view trim_ascii(std::string_view s) noexcept
{
    while (!s.empty())
    {
        const unsigned char c = static_cast<unsigned char>(s.front());
        if (!std::isspace(c))
        {
            break;
        }

        s.remove_prefix(1);
    }

    while (!s.empty())
    {
        const unsigned char c = static_cast<unsigned char>(s.back());
        if (!std::isspace(c))
        {
            break;
        }

        s.remove_suffix(1);
    }

    return s;
}

/// Returns `true` if `c` is a PDF token boundary: whitespace, a PDF delimiter,
/// or end-of-buffer. Used to prevent false keyword matches embedded in larger
/// tokens (e.g. `startxref` inside `startxrefGarbage`).
[[nodiscard]] constexpr bool is_token_boundary(char c) noexcept
{
    switch (c)
    {
        case '\0':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case ' ':
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

[[nodiscard]] inline bool starts_with_token(std::string_view line, std::string_view token) noexcept
{
    line = trim_ascii(strip_line_endings(line));

    if (line.size() < token.size())
    {
        return false;
    }

    if (!line.starts_with(token))
    {
        return false;
    }

    // The character after the token must be a token boundary so we do not
    // match keywords that are merely prefixes of a longer token.
    return line.size() == token.size() || is_token_boundary(line[token.size()]);
}

/// Find the last occurrence of "endobj" in source, verifying that the
/// character after the match is a token boundary (whitespace, PDF
/// delimiter, or end-of-buffer).  Returns `npos` if not found.
///
/// This prevents false matches inside string literals, name objects,
/// and other content that may contain the raw bytes \c e,n,d,o,b,j.
[[nodiscard]] inline std::size_t find_endobj(std::string_view source) noexcept
{
    constexpr std::string_view needle{"endobj"};

    std::size_t search_from = source.size();
    while (search_from >= needle.size())
    {
        const auto pos = source.rfind(needle, search_from - 1);
        if (pos == std::string_view::npos)
            return std::string_view::npos;

        const auto after = pos + needle.size();
        if (after >= source.size() || is_token_boundary(source[after]))
            return pos;

        search_from = pos;
    }

    return std::string_view::npos;
}

[[nodiscard]] inline std::optional<std::size_t> parse_size_t(std::string_view s) noexcept
{
    s = trim_ascii(strip_line_endings(s));
    if (s.empty())
    {
        return std::nullopt;
    }

    std::size_t object = 0;

    const char* begin = s.data();

    /// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* end = s.data() + s.size();

    auto [ptr, ec] = std::from_chars(begin, end, object);
    if (ec != std::errc{})
    {
        return std::nullopt;
    }

    return object;
}

[[nodiscard]] inline std::optional<std::uint32_t> parse_u32(std::string_view text) noexcept
{
    std::uint32_t object{};

    const char* first = text.data();

    /// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* last = text.data() + text.size();

    auto [ptr, ec] = std::from_chars(first, last, object);

    if (ec != std::errc{} || ptr != last)
    {
        return std::nullopt;
    }

    return object;
}

[[nodiscard]] inline std::optional<std::uint16_t> parse_u16(std::string_view text) noexcept
{
    const auto v32 = parse_u32(text);
    if (!v32.has_value() || *v32 > 0xFFFFu)
    {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(*v32);
}

[[nodiscard]] inline std::string escape_literal_string(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (const char ch : value)
    {
        switch (ch)
        {
            case '\\':
                escaped += "\\\\";
                break;
            case '(':
                escaped += "\\(";
                break;
            case ')':
                escaped += "\\)";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            default:
                escaped += ch;
                break;
        }
    }

    return escaped;
}
[[nodiscard]] inline std::string unescape_literal_string(std::string_view value)
{
    std::string unescaped;
    unescaped.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] != '\\' || i + 1 >= value.size())
        {
            unescaped += value[i];
            continue;
        }

        ++i;
        switch (value[i])
        {
            case 'n':
                unescaped += '\n';
                break;
            case 'r':
                unescaped += '\r';
                break;
            case 't':
                unescaped += '\t';
                break;
            case 'b':
                unescaped += '\b';
                break;
            case 'f':
                unescaped += '\f';
                break;
            case '(':
                unescaped += '(';
                break;
            case ')':
                unescaped += ')';
                break;
            case '\\':
                unescaped += '\\';
                break;
            case '\n':
                break;
            case '\r':
                if (i + 1 < value.size() && value[i + 1] == '\n')
                    ++i;
                break;
            default:
            {
                if (value[i] >= '0' && value[i] <= '7')
                {
                    int octal = value[i] - '0';
                    int count = 1;
                    while (count < 3 && i + 1 < value.size() && value[i + 1] >= '0' &&
                           value[i + 1] <= '7')
                    {
                        octal = octal * 8 + (value[++i] - '0');
                        ++count;
                    }
                    if (octal <= 255)
                        unescaped += static_cast<char>(octal);
                }
                else
                {
                    unescaped += value[i];
                }
                break;
            }
        }
    }

    return unescaped;
}

[[nodiscard]] inline bool name_byte_needs_escape(unsigned char c) noexcept
{
    if (c < 0x21 || c > 0x7E)
        return true;

    switch (c)
    {
        case '#': // 0x23
        case '%': // 0x25
        case '(': // 0x28
        case ')': // 0x29
        case '/': // 0x2F
        case '<': // 0x3C
        case '>': // 0x3E
        case '[': // 0x5B
        case ']': // 0x5D
        case '{': // 0x7B
        case '}': // 0x7D
            return true;
        default:
            return false;
    }
}

[[nodiscard]] inline std::string escape_name(std::string_view value)
{
    constexpr auto hex_char = [](unsigned int nibble) noexcept -> char
    {
        return nibble < 10 ? static_cast<char>('0' + nibble) : static_cast<char>('A' + nibble - 10);
    };

    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (const unsigned char ch : value)
    {
        if (name_byte_needs_escape(ch))
        {
            escaped += '#';
            escaped += hex_char((ch >> 4) & 0xF);
            escaped += hex_char(ch & 0xF);
        }
        else
        {
            escaped += static_cast<char>(ch);
        }
    }

    return escaped;
}

[[nodiscard]] inline std::string unescape_name(std::string_view value)
{
    constexpr auto hex_digit = [](unsigned char c) noexcept -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    std::string unescaped;
    unescaped.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '#' && i + 2 < value.size())
        {
            const int hi = hex_digit(static_cast<unsigned char>(value[i + 1]));
            const int lo = hex_digit(static_cast<unsigned char>(value[i + 2]));
            if (hi >= 0 && lo >= 0)
            {
                unescaped += static_cast<char>(static_cast<unsigned char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        unescaped += value[i];
    }

    return unescaped;
}
} // namespace ripper::pdf::core::text
