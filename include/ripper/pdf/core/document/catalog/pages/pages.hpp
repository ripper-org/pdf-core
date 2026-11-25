#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_size.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"

#include <cstdint>
#include <functional>

namespace ripper::pdf::core
{
/// Typed view over a PDF pages tree indirect_object (/Type /Pages).
///
/// Non-owning wrapper around an `indirect_object` stored in the cross-reference table.
/// Ownership remains with the `cross_reference_entry` that holds the indirect_object.
///
/// Page indices used by this class are relative to this node's subtree, so a root
/// `/Pages` view works in document order while a nested node counts only its own
/// descendants.
class pages : public object_view
{
public:
    explicit pages(indirect_object& obj) noexcept;

    /// Returns the total page count from the /Count entry.
    [[nodiscard]] std::uint64_t count() const;

    /// Returns an optional page view for the page at `index` in the page tree, or
    /// `std::nullopt` if the page does not exist.
    ///
    /// The index starts at 0 and must be less than the total page count. Pages are
    /// ordered according to a depth-first traversal of the page tree.
    [[nodiscard]] std::optional<class page> page(std::uint64_t index);

    /// Returns an optional page view for the page based on an indirect reference,
    /// or `std::nullopt` if the page does not exist.
    [[nodiscard]] std::optional<class page> page(indirect_reference ref);

    /// Executes `callback` for each page in index order.
    ///
    /// Throws if callback is empty or if an expected page cannot be resolved.
    void each(const std::function<void(class page&)>& callback);

    /// Creates a page at the end of this node's /Kids.
    ///
    /// The default /MediaBox is US Letter; pass a `page_size` or a rectangle to
    /// override it. Negative extents are normalized.
    [[nodiscard]] class page append_page(const geometry::rect& box = media_box(page_size::a4));

    /// Creates a page of `size` at the end of this node's /Kids.
    [[nodiscard]] class page append_page(page_size size,
                                         page_orientation orientation = page_orientation::portrait);

    /// Creates a page at the beginning of this node's /Kids.
    [[nodiscard]] class page prepend_page(const geometry::rect& box = media_box(page_size::a4));

    /// Creates a page of `size` at the beginning of this node's /Kids.
    [[nodiscard]] class page
    prepend_page(page_size size, page_orientation orientation = page_orientation::portrait);

    /// Creates a page before position `index` in this node's subtree.
    ///
    /// `index == count()` appends. When `index` falls inside a child /Pages node,
    /// the page is inserted into that subtree and its /Parent points at the
    /// subtree node, not at this one.
    ///
    /// @throws logic_exception if `index > count()`.
    [[nodiscard]] class page insert_page(std::uint64_t index,
                                         const geometry::rect& box = media_box(page_size::a4));

    /// Creates a page of `size` before position `index` in this node's subtree.
    [[nodiscard]] class page insert_page(std::uint64_t index, page_size size,
                                         page_orientation orientation = page_orientation::portrait);

    /// Removes the page at `index` from the page tree.
    ///
    /// Detaches it from its parent's /Kids, decrements /Count on the parent and
    /// every ancestor, and marks the page object deleted in the cross-reference
    /// table. Out-of-range indices are a no-op.
    void remove(std::uint64_t index);

    /// Deletes the page and all child objects at `index` in document order.
    ///
    /// Not implemented yet.
    void prune(std::uint64_t index);
};
} // namespace ripper::pdf::core
