#pragma once

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/util/byte_count.hpp"
#include "ripper/pdf/core/util/byte_offset.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
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
[[nodiscard]] byte_count object_stream_first(const dictionary_object& dict,
                                             std::size_t content_size);

/// A fully validated object stream layout.
///
/// Construction is only possible through `parse()`, so every exposed range has
/// already been checked against the decoded payload. Entries expose a borrowed
/// body span instead of separate offset/length integers: callers parse the
/// bytes they were given rather than reconstructing a range themselves.
///
/// The layout borrows from the decoded payload passed to `parse()` and must not
/// outlive it.
class object_stream_layout
{
public:
    /// One object described by the object stream header.
    class entry
    {
    public:
        /// Passkey that restricts entry construction to object_stream_layout.
        struct layout_key
        {
        private:
            layout_key() = default;

            friend class object_stream_layout;
        };

        entry(layout_key, std::uint32_t object_number, byte_offset offset, byte_count length,
              std::span<const std::byte> body) noexcept
            : object_number_{object_number}, offset_{offset}, length_{length}, body_{body}
        {
        }

        [[nodiscard]] std::uint32_t object_number() const noexcept
        {
            return object_number_;
        }

        /// Absolute offset of the body in the decoded payload.
        [[nodiscard]] byte_offset offset() const noexcept
        {
            return offset_;
        }

        /// Validated length of the body.
        [[nodiscard]] byte_count length() const noexcept
        {
            return length_;
        }

        /// Borrowed view of the object's bytes in the decoded payload.
        [[nodiscard]] std::span<const std::byte> body() const noexcept
        {
            return body_;
        }

    private:
        std::uint32_t object_number_;
        byte_offset offset_;
        byte_count length_;
        std::span<const std::byte> body_;
    };

    /// Parse and validate the complete object stream header.
    ///
    /// @throws parse_exception on any malformed or out-of-bounds header.
    [[nodiscard]] static object_stream_layout parse(std::span<const std::byte> content,
                                                    const dictionary_object& dict,
                                                    const resource_limits& limits);

    /// Number of objects declared by the header.
    [[nodiscard]] std::uint32_t count() const noexcept
    {
        return static_cast<std::uint32_t>(entries_.size());
    }

    /// Offset of the first object body within the decoded payload.
    [[nodiscard]] byte_count first() const noexcept
    {
        return first_;
    }

    /// Validated entries in header order.
    [[nodiscard]] std::span<const entry> entries() const noexcept
    {
        return entries_;
    }

private:
    object_stream_layout(byte_count first, std::vector<entry> entries);

    byte_count first_;
    std::vector<entry> entries_;
};
} // namespace ripper::pdf::core
