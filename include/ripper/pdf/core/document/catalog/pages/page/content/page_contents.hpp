#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
/// The /Contents entry of a page (ISO 32000-1 §7.7.3.3).
///
/// Wrapper around the concept of single or multiple content streams, this class
/// acts as a common interface to interact with the page content as one logical
/// unit.
class page_contents
{
public:
    page_contents() noexcept = default;

    /// @param streams  referenced streams, in document order.
    /// @param is_array whether /Contents was an array (even a one-element one).
    explicit page_contents(std::vector<content_stream> streams) noexcept;

    /// Returns `true` if /Contents is absent or references no streams.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns `true` if /Contents contains multiple streams.
    ///
    /// For simplicity's sake, the library considers an array with a single
    /// element the same as a single stream.
    [[nodiscard]] bool is_multiple() const noexcept;

    /// Returns all physical streams, in document order.
    [[nodiscard]] std::span<const content_stream> streams() const noexcept;

private:
    std::vector<content_stream> streams_;
};
} // namespace ripper::pdf::core
