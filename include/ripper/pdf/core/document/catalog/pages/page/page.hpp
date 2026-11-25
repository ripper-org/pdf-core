#pragma once

#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"

#include <cstdint>

namespace ripper::pdf::core
{
class pages;

class page : public object_view
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
    /// a /Contents entry gets an empty stream allocated on first access; a null
    /// /Contents is reported as empty.
    ///
    /// @throws logic_exception if the page is not a dictionary or the new
    ///         content stream cannot be committed.
    [[nodiscard]] page_contents content();

    /// Rebinds the page and returns a canvas over a new content stream.
    ///
    /// `canvas_mode::append` adds the stream at the end of /Contents,
    /// `prepend` at the beginning, and `replace` discards the existing /Contents
    /// and installs the new stream as the only one.
    ///
    /// @throws logic_exception if the page is not a dictionary or the stream
    ///         cannot be committed.
    [[nodiscard]] canvas canvas(canvas_mode mode = canvas_mode::append);
};
} // namespace ripper::pdf::core
