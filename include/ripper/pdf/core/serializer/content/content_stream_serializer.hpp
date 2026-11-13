#pragma once

#include "ripper/pdf/core/content/content_stream_instructions.hpp"

#include <cstddef>
#include <vector>

namespace ripper::pdf::core
{
/// Serializes content-stream instructions back into content-stream bytes.
///
/// The inverse of `content_stream_parser`: operands are written with the regular
/// PDF object grammar, operators follow their operands, and inline-image data is
/// emitted verbatim between `ID` and `EI`. Each item is terminated by a line
/// break, which also guarantees token separation for round-trips.
class content_stream_serializer
{
public:
    content_stream_serializer() = delete;

    /// Serialize every item in stream order.
    ///
    /// @throws logic_exception if an operand cannot be serialized (for example a
    ///         non-finite real number).
    [[nodiscard]] static std::vector<std::byte>
    serialize(const content_stream_instructions& instructions);
};
} // namespace ripper::pdf::core
