#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/instruction_stream.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"

namespace ripper::pdf::core
{
class document;
class stream_object;

/// Typed view over a PDF content stream (ISO 32000-1 §7.8.2).
///
/// A content stream is the stream object referenced by a page's `/Contents`
/// entry (or by an element of its `/Contents` array). It is the PDF object that
/// holds the drawing instructions; reading those instructions lazily is the job
/// of `instruction_stream`.
///
/// Non-owning wrapper around an `indirect_object` stored in the cross-reference
/// table. Ownership remains with the `cross_reference_entry` that holds the
/// indirect_object.
class content_stream : public indirect_object_view
{
public:
    explicit content_stream(indirect_object& obj) noexcept;

    /// Allocates a new, empty content stream and commits it to the active revision.
    ///
    /// The stream gets `/Length 0` but is not referenced by any page yet; attach
    /// it with `page_contents::append`, `insert`, or `replace`.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static content_stream create(document& doc);

    /// Returns the underlying stream, or `nullptr` if the object is not a stream.
    [[nodiscard]] stream_object* stream() noexcept;
    [[nodiscard]] const stream_object* stream() const noexcept;

    /// Returns a lazy view over the decoded instructions of this stream.
    ///
    /// The stream is decoded on demand; its raw bytes are preserved.
    ///
    /// @throws logic_exception if the object is not a stream.
    [[nodiscard]] instruction_stream instructions();
};
} // namespace ripper::pdf::core
