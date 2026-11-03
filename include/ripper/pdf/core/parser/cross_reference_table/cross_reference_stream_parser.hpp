#pragma once

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/xref_field_width.hpp"
#include "ripper/pdf/core/util/byte_offset.hpp"

#include <cstdint>
#include <vector>

namespace ripper::pdf::core
{

/// Parses a cross-reference stream (PDF 1.5+ /Type /XRef) into a cross_reference_section.
///
/// Xref streams use a binary format instead of the traditional text-based `xref` keyword.
/// The stream dictionary_object contains `/W` (column widths), `/Index` (subsection ranges),
/// and the stream data is decoded using the standard filter pipeline.
///
/// @see PDF spec §7.5.8
class cross_reference_stream_parser
{
public:
    /// Parse a decoded xref stream object into a cross_reference_section.
    ///
    /// The stream must already be decoded (i.e. `stream_object::content()` called).
    /// Reads `/W`, `/Index`, `/Size` from the dictionary_object and interprets the binary
    /// stream data according to the xref stream format.
    ///
    /// @throws parse_exception if required keys are missing or data is malformed.
    [[nodiscard]] static cross_reference_section parse(const stream_object& stream_obj);

private:
    struct column_widths
    {
        xref_field_width w0 = xref_field_width::from_integer(1);
        xref_field_width w1 = xref_field_width::from_integer(0);
        xref_field_width w2 = xref_field_width::from_integer(0);
    };

    struct subsection_range
    {
        std::uint32_t first;
        std::uint32_t count;
    };

    [[nodiscard]] static column_widths parse_w(const stream_object& stream_obj);
    [[nodiscard]] static std::vector<subsection_range> parse_index(const stream_object& stream_obj,
                                                                   std::uint32_t size);
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] static std::uint64_t read_field(std::span<const std::byte> data,
                                                  byte_offset offset, xref_field_width width);
};

} // namespace ripper::pdf::core
