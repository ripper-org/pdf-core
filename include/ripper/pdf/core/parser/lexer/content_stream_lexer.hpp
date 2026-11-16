#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_item.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ripper::pdf::core
{
/// The kind of token produced by `content_stream_lexer`.
///
/// Content streams share the PDF lexical conventions (§7.2) but not the object
/// grammar: bare keywords are operators, indirect references are not allowed,
/// and inline images are a construct of their own (§7.8.2, §8.9.7).
enum class content_stream_token_kind : std::uint8_t
{
    end_of_stream,
    operand,
    operator_name,
    inline_image
};

/// One token of a content stream.
///
/// Exactly one payload member is meaningful for a given `kind`:
/// - `operand` for `content_stream_token_kind::operand`
/// - `operator_name` for `content_stream_token_kind::operator_name`
/// - `image` for `content_stream_token_kind::inline_image`
struct content_stream_token
{
    content_stream_token_kind kind = content_stream_token_kind::end_of_stream;
    std::string operator_name;
    object operand;
    inline_image image;
};

/// Tokenizes content-stream bytes (ISO 32000-1 §7.8.2).
///
/// TODO: eventually, we may rewrite the lexer/parsing logic to reuse some object parsing
/// logic. Opting to leave them separate for now to ensure the behavior is consistent.
class content_stream_lexer
{
public:
    /// Construct a lexer over the given bytes. The bytes must outlive the lexer.
    ///
    /// @param max_depth maximum nesting depth for array/dictionary operands.
    explicit content_stream_lexer(std::span<const std::byte> data,
                                  std::size_t max_depth = resource_limits{}.max_parse_depth);

    /// Consume and return the next token.
    ///
    /// @throws parse_exception if the bytes do not follow the content-stream
    ///         grammar, for example an unterminated inline image.
    [[nodiscard]] content_stream_token next();

private:
    [[nodiscard]] bool at_end() const noexcept;

    void skip_whitespace_and_comments();

    /// Read a bare keyword, stopping at white space or a delimiter.
    [[nodiscard]] std::string_view read_keyword();

    /// Parse one operand, recursing through arrays and dictionaries.
    [[nodiscard]] object parse_operand(std::size_t depth);

    [[nodiscard]] object parse_number();
    [[nodiscard]] object parse_name();
    [[nodiscard]] object parse_literal_string();
    [[nodiscard]] object parse_hex_string();
    [[nodiscard]] array_object parse_array(std::size_t depth);
    [[nodiscard]] dictionary_object parse_dictionary(std::size_t depth);

    /// Scan an inline image after its `BI` operator has been consumed.
    [[nodiscard]] inline_image scan_inline_image();

    std::string_view content_;
    std::size_t position_ = 0;
    std::size_t max_depth_;
};
} // namespace ripper::pdf::core
