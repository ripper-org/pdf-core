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

/// An ordered sequence of content-stream instructions and inline images.
///
/// This is the 1:1 representation of a PDF content stream's data (§7.8.2). It is
/// a value type: copies are deep and independent, so a stream can be built,
/// inspected, diffed, and serialized without being tied to a document.
///
/// The stream is deliberately flat: `q`/`Q`, `BT`/`ET`, `BMC`/`BDC`/`EMC`, and
/// `BX`/`EX` are stack operations, not nesting. Resolving them into a tree is a
/// job for a read-side analyzer, never for this container.
class content_stream
{
public:
    using container_type = std::vector<content_item>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    /// Construct an empty content stream.
    content_stream() noexcept = default;

    /// Construct a content stream from an existing item sequence.
    explicit content_stream(container_type items) noexcept;

    /// Construct a content stream from instructions only (no inline images).
    explicit content_stream(std::vector<content_operation> operations) noexcept;

    /// Append an instruction to the end of the stream.
    void append(content_operation operation);

    /// Append an inline image to the end of the stream.
    void append(inline_image image);

    /// Insert `operation` before position `index`.
    ///
    /// @throws std::out_of_range if `index` is greater than `size()`.
    void insert(std::size_t index, content_operation operation);

    /// Insert `image` before position `index`.
    ///
    /// @throws std::out_of_range if `index` is greater than `size()`.
    void insert(std::size_t index, inline_image image);

    /// Erase the item at `index`.
    ///
    /// @throws std::out_of_range if `index` is not less than `size()`.
    void erase(std::size_t index);

    /// Access the item at `index` with bounds checking.
    ///
    /// @throws std::out_of_range if `index` is not less than `size()`.
    [[nodiscard]] content_item& at(std::size_t index);
    [[nodiscard]] const content_item& at(std::size_t index) const;

    /// Access the item at `index`. No bounds checking.
    [[nodiscard]] content_item& operator[](std::size_t index) noexcept;
    [[nodiscard]] const content_item& operator[](std::size_t index) const noexcept;

    /// Returns the number of items in the stream (instructions + inline images).
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns `true` if the stream has no items.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns the number of instructions (excludes inline images).
    [[nodiscard]] std::size_t operation_count() const noexcept;

    /// Returns the number of inline images (excludes instructions).
    [[nodiscard]] std::size_t inline_image_count() const noexcept;

    /// Remove all items.
    void clear() noexcept;

    /// Returns the raw underlying sequence for full traversal or serialization.
    [[nodiscard]] container_type& items() noexcept;

    /// Returns a const reference to the raw underlying sequence.
    [[nodiscard]] const container_type& items() const noexcept;

    /// Returns an iterator to the beginning.
    [[nodiscard]] iterator begin() noexcept;

    /// Returns a const iterator to the beginning.
    [[nodiscard]] const_iterator begin() const noexcept;

    /// Returns an iterator to the end.
    [[nodiscard]] iterator end() noexcept;

    /// Returns a const iterator to the end.
    [[nodiscard]] const_iterator end() const noexcept;

    /// Returns a const iterator to the beginning.
    [[nodiscard]] const_iterator cbegin() const noexcept;

    /// Returns a const iterator to the end.
    [[nodiscard]] const_iterator cend() const noexcept;

private:
    container_type items_;
};
} // namespace ripper::pdf::core
