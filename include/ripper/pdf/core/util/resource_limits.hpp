#pragma once

#include <cstddef>

namespace ripper::pdf::core
{
/// Configurable resource limits applied while parsing untrusted PDF data.
///
/// The defaults are intentionally generous enough for ordinary documents while
/// bounding work that a malformed document could otherwise make unbounded.
/// Override them per document with `document::set_resource_limits()`.
struct resource_limits
{
    /// Maximum bytes produced by decoding a single stream (cumulative across filters).
    std::size_t max_decoded_stream_bytes = 256U * 1024U * 1024U;

    /// Maximum size of one predictor row buffer.
    std::size_t max_predictor_row_bytes = 16U * 1024U * 1024U;

    /// Maximum number of objects declared by an object stream's /N entry.
    std::size_t max_object_stream_objects = 1'000'000U;

    /// Maximum nesting depth for arrays and dictionaries while parsing values.
    std::size_t max_parse_depth = 256U;

    /// Maximum number of indirect objects that may be resolved recursively.
    std::size_t max_resolution_depth = 64U;

    /// Maximum depth of the page tree during traversal.
    std::size_t max_page_tree_depth = 256U;
};
} // namespace ripper::pdf::core
