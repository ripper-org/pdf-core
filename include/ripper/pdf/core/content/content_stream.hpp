#pragma once

#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <string>
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
    /// Operands consumed by `operator_name`, in stream order.
    std::vector<object> operands;

    /// Operator name without delimiters (e.g. "cm", "Tj", "f*").
    std::string operator_name;
};

/// An ordered sequence of content-stream instructions.
///
/// This is the 1:1 representation of a PDF content stream's data (§7.8.2). It is
/// a value type: copies are deep and independent, so a stream can be built,
/// inspected, diffed, and serialized without being tied to a document.
class content_stream
{
public:
    /// Type alias for the underlying storage.
    using storage_type = std::vector<content_operation>;

    /// Iterator types for range-based usage.
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    /// Construct an empty content stream.
    content_stream() noexcept = default;

    /// Construct a content stream from an existing operation sequence.
    explicit content_stream(storage_type operations) noexcept;

    /// Append an operation to the end of the stream.
    void append(content_operation operation);

    /// Insert `operation` before position `index`.
    ///
    /// @throws std::out_of_range if `index` is greater than `size()`.
    void insert(std::size_t index, content_operation operation);

    /// Erase the operation at `index`.
    ///
    /// @throws std::out_of_range if `index` is not less than `size()`.
    void erase(std::size_t index);

    /// Access the operation at `index` with bounds checking.
    ///
    /// @throws std::out_of_range if `index` is not less than `size()`.
    [[nodiscard]] content_operation& at(std::size_t index);
    [[nodiscard]] const content_operation& at(std::size_t index) const;

    /// Access the operation at `index`. No bounds checking.
    [[nodiscard]] content_operation& operator[](std::size_t index) noexcept;
    [[nodiscard]] const content_operation& operator[](std::size_t index) const noexcept;

    /// Returns the number of operations in the stream.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns `true` if the stream has no operations.
    [[nodiscard]] bool empty() const noexcept;

    /// Remove all operations.
    void clear() noexcept;

    /// Returns the raw underlying vector for full traversal or serialization.
    [[nodiscard]] storage_type& operations() noexcept;

    /// Returns a const reference to the raw underlying vector.
    [[nodiscard]] const storage_type& operations() const noexcept;

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
    storage_type operations_;
};
} // namespace ripper::pdf::core
