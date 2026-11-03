#include "ripper/pdf/core/parser/value_parsing.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace ripper::pdf::core
{
namespace
{

/// Decode the raw contents of a hex string (`<...>`) into bytes.
///
/// Whitespace between hex digits is ignored. A trailing lone nibble is padded
/// with a low zero per PDF 32000-1 §7.3.4.3.
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

[[nodiscard]] object parse_value_impl(pdf_lexer& lexer, std::size_t depth, std::size_t max_depth);
[[nodiscard]] array_object parse_array_impl(pdf_lexer& lexer, std::size_t depth,
                                            std::size_t max_depth);
[[nodiscard]] dictionary_object parse_dictionary_impl(pdf_lexer& lexer, std::size_t depth,
                                                      std::size_t max_depth);

void check_depth(std::size_t depth, std::size_t max_depth)
{
    if (depth > max_depth)
        throw parse_exception{"Maximum parsing depth exceeded"};
}

} // namespace

indirect_reference parse_indirect_reference(pdf_lexer& lexer)
{
    auto obj = lexer.next();
    auto gen = lexer.next();
    auto marker = lexer.next();

    if (obj.type != lexer_token_type::integer || gen.type != lexer_token_type::integer ||
        marker.type != lexer_token_type::keyword || marker.lexeme != "R")
        throw parse_exception{"Invalid indirect reference"};

    const auto obj_num = text::parse_size_t(obj.lexeme);
    const auto gen_num = text::parse_size_t(gen.lexeme);

    if (!obj_num || !gen_num || *obj_num > std::numeric_limits<std::uint32_t>::max() ||
        *gen_num > std::numeric_limits<std::uint16_t>::max())
        throw parse_exception{"Indirect reference numbers are out of range"};

    return indirect_reference{static_cast<std::uint32_t>(*obj_num),
                              static_cast<std::uint16_t>(*gen_num)};
}

namespace
{

array_object parse_array_impl(pdf_lexer& lexer, std::size_t depth, std::size_t max_depth)
{
    check_depth(depth, max_depth);

    array_object arr;
    while (true)
    {
        auto p = lexer.peek();

        if (p.type == lexer_token_type::array_end)
        {
            (void)lexer.next();
            break;
        }

        if (p.type == lexer_token_type::eof)
            throw parse_exception{"Unexpected EOF inside array"};

        auto v = parse_value_impl(lexer, depth, max_depth);
        arr.push_back(std::move(v));
    }
    return arr;
}

dictionary_object parse_dictionary_impl(pdf_lexer& lexer, std::size_t depth, std::size_t max_depth)
{
    check_depth(depth, max_depth);

    dictionary_object dict;
    while (true)
    {
        auto p = lexer.peek();

        if (p.type == lexer_token_type::dictionary_end)
        {
            (void)lexer.next();
            break;
        }

        if (p.type == lexer_token_type::eof)
            throw parse_exception{"Unexpected EOF inside nested dictionary"};

        if (p.type != lexer_token_type::name)
        {
            lexer.skip_value();
            continue;
        }

        const auto key = lexer.next();
        auto val = parse_value_impl(lexer, depth, max_depth);

        dict.set(text::unescape_name(key.lexeme), std::move(val));
    }
    return dict;
}

object parse_value_impl(pdf_lexer& lexer, std::size_t depth, std::size_t max_depth)
{
    check_depth(depth, max_depth);

    auto p = lexer.peek();

    if (p.type == lexer_token_type::integer)
    {
        // Peek ahead to detect `obj gen R` before consuming anything.
        auto p1 = lexer.peek(1);
        auto p2 = lexer.peek(2);
        if (p1.type == lexer_token_type::integer && p2.type == lexer_token_type::keyword &&
            p2.lexeme == "R")
        {
            auto ref = parse_indirect_reference(lexer);
            return object{ref};
        }

        const auto tok = lexer.next();
        std::int64_t i = 0;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto [ptr, ec] =
            std::from_chars(tok.lexeme.data(), tok.lexeme.data() + tok.lexeme.size(), i);
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (ec == std::errc{})
            return object{i};
        return object{std::string{tok.lexeme}};
    }

    if (p.type == lexer_token_type::real)
    {
        const auto tok = lexer.next();
        double d = 0.0;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto [ptr, ec] =
            std::from_chars(tok.lexeme.data(), tok.lexeme.data() + tok.lexeme.size(), d);
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (ec == std::errc{})
            return object{d};
        return object{std::string{tok.lexeme}};
    }

    if (p.type == lexer_token_type::name)
    {
        const auto tok = lexer.next();
        return object{name_object{text::unescape_name(tok.lexeme)}};
    }

    if (p.type == lexer_token_type::literal_string)
    {
        const auto tok = lexer.next();
        return object{text::unescape_literal_string(tok.lexeme)};
    }

    if (p.type == lexer_token_type::hex_string)
    {
        const auto tok = lexer.next();

        std::vector<std::byte> original;
        original.reserve(tok.lexeme.size());
        for (const char c : tok.lexeme)
            original.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));

        return object{string_object{decode_hex_string(tok.lexeme), std::move(original),
                                    string_object::encoding::utf8, string_object::form::hex}};
    }

    if (p.type == lexer_token_type::keyword)
    {
        const auto tok = lexer.next();
        if (tok.lexeme == "true")
            return object{boolean_object{true}};

        if (tok.lexeme == "false")
            return object{boolean_object{false}};

        return object{};
    }

    if (p.type == lexer_token_type::array_begin)
    {
        (void)lexer.next();

        auto arr = parse_array_impl(lexer, depth + 1, max_depth);
        return object{std::move(arr)};
    }

    if (p.type == lexer_token_type::dictionary_begin)
    {
        (void)lexer.next();

        auto dict = parse_dictionary_impl(lexer, depth + 1, max_depth);
        return object{std::move(dict)};
    }

    // Skip unrecognized tokens
    lexer.skip_value();

    return object{};
}

} // namespace

array_object parse_array(pdf_lexer& lexer, std::size_t max_depth)
{
    return parse_array_impl(lexer, 0, max_depth);
}

dictionary_object parse_dictionary(pdf_lexer& lexer, std::size_t max_depth)
{
    return parse_dictionary_impl(lexer, 0, max_depth);
}

object parse_value(pdf_lexer& lexer, std::size_t max_depth)
{
    return parse_value_impl(lexer, 0, max_depth);
}

std::string_view extract_object_body(std::string_view content)
{
    pdf_lexer lexer{content};

    // Find the 'obj' keyword token
    lexer_token obj_token;
    while (true)
    {
        auto token = lexer.next();
        if (token.type == lexer_token_type::eof)
            throw parse_exception{"Missing obj keyword in indirect object content"};

        if (token.type == lexer_token_type::keyword && token.lexeme == "obj")
        {
            obj_token = token;
            break;
        }
    }

    /// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto value_start = static_cast<std::size_t>(obj_token.lexeme.data() +
                                                      obj_token.lexeme.size() - content.data());
    /// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    const auto endobj_pos = text::find_endobj(content);
    if (endobj_pos == std::string_view::npos)
        throw parse_exception{"Missing endobj keyword"};

    if (endobj_pos < value_start)
        throw parse_exception{"Malformed indirect object: endobj precedes object content"};

    return content.substr(value_start, endobj_pos - value_start);
}
} // namespace ripper::pdf::core
