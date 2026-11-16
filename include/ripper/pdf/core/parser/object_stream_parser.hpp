#pragma once

#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ripper::pdf::core
{
class document;

/// Parses an Object Stream (PDF 1.5+ /Type /ObjStm) into individual indirect objects.
///
/// Object Streams store multiple indirect objects in a single compressed stream.
/// The stream format is:
///   - Header: N pairs of (object_number byte_offset) separated by whitespace
///   - Body: N objects, each starting at the corresponding byte offset
///
/// @see PDF spec §7.5.8
class object_stream_parser
{
public:
    /// Parse all objects from an object stream.
    ///
    /// The stream is decoded on demand; `raw()` bytes are left untouched.
    /// Returns a vector of indirect objects with their identities set.
    ///
    /// @throws parse_exception if required keys are missing or data is malformed.
    [[nodiscard]] static std::vector<indirect_object> parse(document& doc,
                                                            const stream_object& stream_obj);
};

} // namespace ripper::pdf::core
