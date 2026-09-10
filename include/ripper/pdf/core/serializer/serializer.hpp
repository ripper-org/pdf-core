#pragma once

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/serializer_manager.hpp"

namespace ripper::pdf::core
{
/// High-level PDF serializer facade for a single `document`.
///
/// This type orchestrates serialization by delegating to components managed by
/// `serializer_manager`, and throws on failures.
class serializer
{
public:
    /// Construct a serializer bound to `doc`.
    ///
    /// The serializer stores a reference and does not take ownership of the document.
    explicit serializer(const document& doc);

    /// Return the serializer manager used by this serializer.
    ///
    /// Can be used to replace serializer subcomponents.
    [[nodiscard]] class serializer_manager& manager();

    /// Set the character used for line breaks across all sub-serializers (default `\n`).
    void set_line_break_character(char c);

    /// Set the character used for object-level breaks across all sub-serializers (default `\n`).
    void set_object_break_character(char c);

    /// Serialize a PDF header to a byte buffer.
    [[nodiscard]] std::vector<std::byte> serialize_header(const header& header);

    /// Serialize a PDF indirect object to a byte buffer.
    [[nodiscard]] std::vector<std::byte> serialize_indirect_object(const indirect_object& obj);

    /// Serialize a single cross-reference section to a byte buffer.
    ///
    /// Emits the `xref` block for `section` only. Use this when interleaving object
    /// writes with xref writes during an incremental update.
    ///
    /// `trailer` carries the trailer dictionary_object entries that the compressed serializer
    /// merges into the xref stream dictionary_object when the section is compressed; the
    /// traditional serializer ignores it.
    [[nodiscard]] std::vector<std::byte>
    serialize_cross_reference_section(const cross_reference_section& section,
                                      const trailer& trailer);

    /// Serialize a PDF trailer block to a byte buffer.
    ///
    /// Emits `trailer\n<<dict>>\nstartxref\n<xref_offset>\n%%EOF\n`.
    /// The trailer dictionary_object is serialized as-is, no keys are stripped.
    [[nodiscard]] std::vector<std::byte> serialize_trailer(const trailer& t,
                                                           std::uint64_t xref_offset);

    /// Serialize a single PDF revision to a byte buffer.
    ///
    /// When `rev.section().is_compressed()` is true, emits a compressed xref stream indirect
    /// object (with the trailer dictionary_object merged into the stream dictionary_object)
    /// followed by `startxref\n<xref_offset>\n%%EOF`. Otherwise, emits the traditional `xref` block
    /// followed by the trailer block (`trailer\n<<dict>>\nstartxref\n<xref_offset>\n%%EOF`).
    ///
    /// `xref_offset` is the byte offset in the output file where the serialized xref
    /// block (or xref stream indirect object) begins. It is written after `startxref`
    /// so reader applications can locate the revision.
    [[nodiscard]] std::vector<std::byte> serialize_revision(const revision& rev,
                                                            std::uint64_t xref_offset);

private:
    const document& document_;

    std::unique_ptr<class serializer_manager> manager_;
};
} // namespace ripper::pdf::core
