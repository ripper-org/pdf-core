#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/rotation.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"

#include <cstdint>

namespace ripper::pdf::core
{
class pages;

class page : public indirect_object_view
{
public:
    explicit page(indirect_object& obj) noexcept;

    /// Returns the parent /Pages node of this page.
    ///
    /// Per the PDF spec (§7.7.3.3) the /Parent of every /Page leaf is always a /Pages
    /// node, so the returned view exposes the full pages API (count, each,
    /// append_page, remove…).
    ///
    /// @throws logic_exception if /Parent is absent, cannot be resolved, or does
    ///         not point at a /Type /Pages dictionary.
    [[nodiscard]] pages parent();

    /// Returns this page's position within its parent /Kids, counted in pages.
    ///
    /// The index is relative to the immediate parent node, matching
    /// `pages::remove()` and `pages::insert_page()` on that node.
    ///
    /// @throws logic_exception if the page has no /Parent or is not present in
    ///         its parent's /Kids.
    [[nodiscard]] std::uint64_t index();

    /// Removes this page from the page tree.
    ///
    /// Detaches the page from its parent's /Kids, decrements /Count on the parent
    /// and every ancestor, and marks the page object deleted. Named after
    /// `pages::remove()` because `delete` is a keyword.
    ///
    /// The view is invalid after this call.
    ///
    /// @throws logic_exception if the page has no /Parent or is not present in
    ///         its parent's /Kids.
    void remove();

    /// Returns the page's /Contents entry.
    ///
    /// /Contents is either a single content stream or an array of content
    /// streams (ISO 32000-1 §7.7.3.3). Both forms are exposed through the same
    /// view, with is_array() reporting which form the file used. A page without
    /// a /Contents entry gets an empty array created on first access and is
    /// reported as empty; no content stream is allocated until content is
    /// actually written (for example through `brush()`).
    ///
    /// @throws logic_exception if the page is not a dictionary or the new slot
    ///         cannot be created.
    [[nodiscard]] page_contents content();

    /// Rebinds the page and returns a brush over a new content stream.
    ///
    /// `brush_mode::append` adds the stream at the end of /Contents,
    /// `prepend` at the beginning, and `replace` discards the existing /Contents
    /// and installs the new stream as the only one.
    ///
    /// @throws logic_exception if the page is not a dictionary or the stream
    ///         cannot be committed.
    [[nodiscard]] graphics::brush brush(graphics::brush_mode mode = graphics::brush_mode::append);

    /// Returns the effective /MediaBox of this page, inheriting from ancestors.
    ///
    /// /MediaBox is an inheritable page attribute (ISO 32000-1 §7.7.3.4), so
    /// when the leaf has no entry of its own the /Parent chain is searched.
    /// The returned rectangle keeps the box origin; corners are normalized so
    /// `width` and `height` are never negative.
    ///
    /// When no /MediaBox exists anywhere in the chain, US Letter is returned,
    /// matching the Acrobat fallback for malformed files.
    ///
    /// @throws logic_exception if the page is not a dictionary or a /Parent
    ///         chain is cyclic.
    [[nodiscard]] geometry::rect media_box();

    /// Returns the effective /CropBox of this page, inheriting from ancestors.
    ///
    /// Falls back to `media_box()` when neither this page nor an ancestor
    /// defines a /CropBox (ISO 32000-1 §14.11.2).
    ///
    /// @throws logic_exception if the page is not a dictionary or a /Parent
    ///         chain is cyclic.
    [[nodiscard]] geometry::rect crop_box();

    /// Returns the effective /Rotate value of this page, inheriting from ancestors.
    ///
    /// The result is normalized to 0, 90, 180 or 270 degrees clockwise. A page
    /// without a /Rotate entry anywhere in the chain reports 0.
    ///
    /// @throws logic_exception if the page is not a dictionary, the value is not
    ///         a number or not a multiple of 90, or a /Parent chain is cyclic.
    [[nodiscard]] page_rotation rotation();

    /// Returns the displayed width of the page in points.
    ///
    /// This is the /MediaBox width after applying `rotation()`: at 90 and 270
    /// degrees the displayed dimensions swap. Use `media_box().width` for the
    /// unrotated extent.
    [[nodiscard]] double width();

    /// Returns the displayed height of the page in points.
    ///
    /// Counterpart of `width()`; see its documentation for the rotation rules.
    [[nodiscard]] double height();

    /// Sets /Rotate on this page.
    ///
    /// The entry is written to this page's dictionary (never to an ancestor)
    /// and normalized into 0, 90, 180 or 270. `0` effectively removes the
    /// rotation, since 0 is also the inherited default.
    ///
    /// @throws logic_exception if `degrees` is not a multiple of 90 or the page
    ///         is not a dictionary.
    void rotate(int degrees);
};

} // namespace ripper::pdf::core
