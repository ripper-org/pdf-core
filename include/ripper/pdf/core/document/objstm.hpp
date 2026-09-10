#pragma once

#include "ripper/pdf/core/document/object/helpers/object_view.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ripper::pdf::core
{
class document;

/// Typed view over an Object Stream (PDF 1.5+ /Type /ObjStm).
///
/// An Object Stream (ISO 32000-1 §7.5.7) is a PDF stream object whose dictionary holds
/// entries such as /Type, /N and /First. This view binds to an `indirect_object` whose
/// value is that stream, and provides domain-specific methods over the stream's
/// dictionary and decoded payload.
///
/// Follows the same pattern as `catalog`, `pages`, and `page`. Inherits from
/// `object_view` and provides domain-specific methods over the stream content.
class objstm : public object_view
{
public:
    /// The byte range of an object within the decoded stream.
    struct object_range
    {
        std::size_t offset; ///< Byte position in decoded stream.
        std::size_t length; ///< Length of the object in bytes.
    };

    explicit objstm(indirect_object& obj) noexcept;

    /// Returns the number of objects stored in this stream (/N).
    [[nodiscard]] std::uint32_t count() const;

    /// Returns the byte offset of the first object in the decoded stream (/First).
    [[nodiscard]] std::uint32_t first_offset() const;

    /// Returns the extension Object Stream (/Extends), if present.
    ///
    /// Resolves the indirect reference through the document's cross-reference
    /// table and returns a typed `objstm` view.
    ///
    /// @throws parse_exception if /Extends points back at this object stream
    ///         (self-referential extension detected).
    [[nodiscard]] std::optional<class objstm> extension();

    /// Finds the byte range of a specific object within the decoded stream.
    ///
    /// Parses the object stream header to locate the object at the given index
    /// and returns its byte range. Returns `std::nullopt` if the index is
    /// out of bounds or the stream is malformed.
    ///
    /// The stream must already be decoded (`content()` called).
    [[nodiscard]] std::optional<object_range> object_offset(std::uint32_t index) const;

    /// Resolves and returns all contained objects.
    ///
    /// Decodes the stream if needed, parses all contained objects, and returns
    /// them as fully resolved indirect objects.
    ///
    /// @throws parse_exception if the stream is malformed or /Type is not /ObjStm.
    [[nodiscard]] std::vector<indirect_object> objects();
};

} // namespace ripper::pdf::core
