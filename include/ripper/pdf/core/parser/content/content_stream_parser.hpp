#pragma once

#include "ripper/pdf/core/content/content_stream_instructions.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <span>

namespace ripper::pdf::core
{
/// Parses content-stream bytes (ISO 32000-1 §7.8.2) into their instructions.
///
/// The parser is the instruction-assembly layer: it groups the operands produced
/// by `content_stream_lexer` into `content_operation` items and preserves inline
/// images in stream order. It is pure and document-independent: it consumes a
/// byte range and returns a detached value, staying agnostic about which stream
/// object the bytes came from.
///
/// ## Leniency
///
/// The parser is strict about the stream *shape* (balanced operands/operators,
/// terminated inline images) because the result is a 1:1 model of the stream.
/// It is lenient about *semantics*: operator arity and operand types are not
/// validated, so unknown operators and forward-spec extensions round-trip.
class content_stream_parser
{
public:
    content_stream_parser() = delete;

    /// Parse decoded content-stream bytes.
    ///
    /// @throws parse_exception if the bytes do not form a well-shaped content
    ///         stream, or if nested values exceed `limits.max_parse_depth`.
    [[nodiscard]] static content_stream_instructions parse(std::span<const std::byte> data,
                                                           const resource_limits& limits = {});
};
} // namespace ripper::pdf::core
