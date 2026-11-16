#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/instruction_stream.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
/// Serializes content-stream instructions back into content-stream bytes.
///
/// Operands are written with the regular PDF object grammar, operators follow
/// their operands, and inline-image data is emitted verbatim between `ID` and
/// `EI`. Each item is terminated by a line break, which also guarantees token
/// separation for round-trips.
///
/// Serialization is the write path: an `instruction_stream` is read lazily, so
/// serializing one does not require materializing the whole page.
class content_stream_serializer
{
public:
    content_stream_serializer() = delete;

    /// Serialize a single instruction.
    ///
    /// @throws logic_exception if an operand cannot be serialized (for example a
    ///         non-finite real number).
    [[nodiscard]] static std::vector<std::byte> serialize(const content_operation& operation);

    /// Serialize a single inline image.
    [[nodiscard]] static std::vector<std::byte> serialize(const inline_image& image);

    /// Serialize one content-stream item.
    [[nodiscard]] static std::vector<std::byte> serialize(const content_item& item);

    /// Serialize a sequence of content-stream items.
    [[nodiscard]] static std::vector<std::byte> serialize(std::span<const content_item> items);

    /// Serialize every item of `stream` in stream order.
    [[nodiscard]] static std::vector<std::byte> serialize(const instruction_stream& stream);
};
} // namespace ripper::pdf::core
