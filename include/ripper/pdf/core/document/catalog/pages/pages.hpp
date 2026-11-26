#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_size.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace ripper::pdf::core
{
class object;

/// Typed view over a PDF pages tree indirect_object (/Type /Pages).
///
/// Non-owning wrapper around an `indirect_object` stored in the cross-reference table.
/// Ownership remains with the `cross_reference_entry` that holds the indirect_object.
///
/// Page indices used by this class are relative to this node's subtree, so a root
/// `/Pages` view works in document order while a nested node counts only its own
/// descendants.
class pages : public indirect_object_view
{
public:
    explicit pages(indirect_object& obj) noexcept;

    /// Returns the total page count from the /Count entry.
    [[nodiscard]] std::uint64_t count() const;

    /// Returns the parent /Pages node, or `std::nullopt` for the root node or
    /// when /Parent is absent or does not point at a /Pages dictionary.
    [[nodiscard]] std::optional<pages> parent();

    /// Returns the value of an inheritable entry defined on this node or the
    /// nearest ancestor.
    ///
    /// Inheritable page attributes (/MediaBox, /CropBox, /Rotate, /Resources, …)
    /// are looked up along the /Parent chain (ISO 32000-1 §7.7.3.4). Indirect
    /// values are resolved. Returns `nullptr` when no node in the chain defines
    /// the entry.
    ///
    /// @throws logic_exception if this node is not a dictionary or the /Parent
    ///         chain is cyclic.
    [[nodiscard]] const object* inherited(std::string_view key);

    /// Returns the /MediaBox defined on this node or the nearest ancestor.
    ///
    /// Returns `std::nullopt` when no node in the chain defines a valid box; it
    /// does not synthesize a default. Callers that need a concrete box can use
    /// `value_or(...)` (Acrobat and PDFium fall back to US Letter).
    ///
    /// @throws logic_exception if this node is not a dictionary or the /Parent
    ///         chain is cyclic.
    [[nodiscard]] std::optional<geometry::rect> media_box();

    /// Returns the /CropBox defined on this node or the nearest ancestor.
    ///
    /// Returns `std::nullopt` when no node in the chain defines a valid box,
    /// without falling back to `media_box()`; that fallback is a rendering
    /// concern (ISO 32000-1 §14.11.2).
    ///
    /// @throws logic_exception if this node is not a dictionary or the /Parent
    ///         chain is cyclic.
    [[nodiscard]] std::optional<geometry::rect> crop_box();

    /// Returns the /Rotate defined on this node or the nearest ancestor.
    ///
    /// The result is normalized to 0, 90, 180 or 270 degrees clockwise.
    /// Returns `std::nullopt` when no node in the chain defines /Rotate.
    ///
    /// @throws logic_exception if the value is not a number or not a multiple
    ///         of 90, or if the /Parent chain is cyclic.
    [[nodiscard]] std::optional<page_rotation> rotation();

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
    [[nodiscard]] class page
    append_page(const geometry::rect& box = ripper::pdf::core::media_box(page_size::a4));

    /// Creates a page of `size` at the end of this node's /Kids.
    [[nodiscard]] class page append_page(page_size size,
                                         page_orientation orientation = page_orientation::portrait);

    /// Creates a page at the beginning of this node's /Kids.
    [[nodiscard]] class page
    prepend_page(const geometry::rect& box = ripper::pdf::core::media_box(page_size::a4));

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
    [[nodiscard]] class page
    insert_page(std::uint64_t index,
                const geometry::rect& box = ripper::pdf::core::media_box(page_size::a4));

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
