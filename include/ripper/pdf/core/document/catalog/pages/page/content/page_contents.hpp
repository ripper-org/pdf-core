#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
/// The /Contents entry of a page (ISO 32000-1 §7.7.3.3).
///
/// The entry is either a single content stream or an array of content streams;
/// both forms denote the same page content, so this view hides the difference.
/// `is_array()` reports the syntactic form and `size()` the number of streams.
///
/// Non-owning: each `content_stream` wraps an indirect_object owned by the
/// document's cross-reference table, which must outlive this view.
class page_contents
{
public:
    using storage_type = std::vector<content_stream>;
    using const_iterator = storage_type::const_iterator;

    page_contents() noexcept = default;

    /// @param streams  referenced streams, in document order.
    /// @param is_array whether /Contents was an array (even a one-element one).
    explicit page_contents(storage_type streams, bool is_array = false) noexcept;

    /// Returns `true` if /Contents is absent or references no streams.
    [[nodiscard]] bool empty() const noexcept
    {
        return streams_.empty();
    }

    /// Returns the number of physical content streams.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return streams_.size();
    }

    /// Returns `true` if /Contents was written as an array.
    [[nodiscard]] bool is_array() const noexcept
    {
        return is_array_;
    }

    /// Returns the stream at `index`. No bounds checking.
    [[nodiscard]] const content_stream& operator[](std::size_t index) const
    {
        return streams_[index];
    }

    /// Returns the stream at `index`.
    /// @throws std::out_of_range if `index` is out of range.
    [[nodiscard]] const content_stream& at(std::size_t index) const
    {
        return streams_.at(index);
    }

    /// Returns all physical streams, in document order.
    [[nodiscard]] std::span<const content_stream> streams() const noexcept
    {
        return streams_;
    }

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return streams_.begin();
    }
    [[nodiscard]] const_iterator end() const noexcept
    {
        return streams_.end();
    }

private:
    storage_type streams_;
    bool is_array_ = false;
};
} // namespace ripper::pdf::core
