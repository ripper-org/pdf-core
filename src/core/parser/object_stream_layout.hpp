#pragma once

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
/// One object described by an object stream header.
struct object_stream_entry
{
    std::uint32_t object_number;
    std::size_t offset; ///< Absolute offset into the decoded stream.
    std::size_t length; ///< Validated length of the object's byte range.
};

/// A fully validated object stream layout.
struct object_stream_layout
{
    std::uint32_t count;
    std::size_t first;
    std::vector<object_stream_entry> entries;
};

/// Read and validate the object stream's /N entry.
///
/// @throws parse_exception if /N is missing, non-integral, non-positive, or
///         exceeds `limits.max_object_stream_objects`.
[[nodiscard]] std::uint32_t object_stream_count(const dictionary_object& dict,
                                                const resource_limits& limits);

/// Read and validate the object stream's /First entry.
///
/// @throws parse_exception if /First is missing, non-integral, negative, or
///         greater than `content_size`.
[[nodiscard]] std::size_t object_stream_first(const dictionary_object& dict,
                                              std::size_t content_size);

/// Parse and validate the complete object stream header.
///
/// Every entry's byte range is validated before it is exposed to callers:
/// offsets are bounded by the physical buffer, additions cannot overflow, and
/// entries must be strictly ordered so no range can underflow.
///
/// @throws parse_exception on any malformed or out-of-bounds header.
[[nodiscard]] object_stream_layout parse_object_stream_layout(std::span<const std::byte> content,
                                                               const dictionary_object& dict,
                                                               const resource_limits& limits);
} // namespace ripper::pdf::core
