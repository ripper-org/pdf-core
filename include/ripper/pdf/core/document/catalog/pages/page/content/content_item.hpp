#pragma once

#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace ripper::pdf::core
{
/// A single content-stream instruction (ISO 32000-1 §7.8.2): one operator and
/// the operands it consumes.
///
/// Operands are stored as ordinary PDF objects so the exact value types supplied
/// by the caller are preserved (integer vs. real, name vs. string, …). The
/// operator name is an open string, which allows unknown and custom operators to
/// round-trip unchanged.
struct content_operation
{
    /// Operator name without delimiters (e.g. "cm", "Tj", "f*").
    std::string operator_name;

    /// Operands consumed by `operator_name`, in stream order.
    std::vector<object> operands;
};

/// An inline image (ISO 32000-1 §8.9.7): `BI <entries> ID <data> EI`.
///
/// Inline images are the only content-stream construct that does not fit the
/// "operator followed by operands" grammar. Their dictionary is a bare run of
/// key/value pairs rather than a dictionary object, and their data is raw bytes
/// that are not a PDF object and must never be escaped or normalized. They are
/// therefore modelled as their own alternative in `content_item` instead of
/// being forced into `content_operation`.
struct inline_image
{
    /// Inline image dictionary entries (/W, /H, /BPC, /CS, /F, /DP, /IM, /L, …).
    dictionary_object dictionary;

    /// Raw image bytes between `ID` and `EI`, preserved exactly as they appear
    /// in the source (or as supplied by the caller).
    std::vector<std::byte> data;
};

/// One element of a content stream: either an instruction or an inline image.
///
/// Keeping the two alternatives distinct makes the §8.9.7 exception explicit in
/// the type system; `content_operation` stays a pure, invariant-free struct.
using content_item = std::variant<content_operation, inline_image>;
} // namespace ripper::pdf::core
