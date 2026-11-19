#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"
#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <vector>

namespace ripper::pdf::core
{
class document;

/// The /Contents entry of a page (ISO 32000-1 §7.7.3.3).
///
/// Wrapper around the concept of single or multiple content streams, this class
/// acts as a common interface to interact with the page content as one logical
/// unit.
///
/// Non-owning view: the wrapped `/Contents` object and the document that owns
/// it must outlive this instance.
class page_contents
{
public:
    /// @param doc     document owning the references inside /Contents.
    /// @param object  the /Contents value (null, an indirect reference, or an
    ///                array of indirect references).
    explicit page_contents(document& doc, object& object) noexcept;

    /// Returns `true` if /Contents is null or an empty array.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns `true` if /Contents is stored as an array, even a one-element one.
    ///
    /// For simplicity's sake, the library considers an array with a single
    /// element the same as a single stream.
    [[nodiscard]] bool is_array() const noexcept;

    /// Resolves /Contents and returns all physical streams, in document order.
    ///
    /// Returns an empty vector when /Contents is null.
    ///
    /// @throws logic_exception if /Contents is not a stream reference or an
    ///         array of stream references, or a reference cannot be resolved.
    [[nodiscard]] std::vector<content_stream> streams() const;

private:
    document& doc_;
    object& object_;
};
} // namespace ripper::pdf::core
