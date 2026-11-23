#pragma once

#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"

namespace ripper::pdf::core
{
class pages;

class page : public object_view
{
public:
    explicit page(indirect_object& obj) noexcept;

    /// Returns the parent /Pages node of this page, or std::nullopt if /Parent is absent
    /// or cannot be resolved.
    ///
    /// Per the PDF spec (§7.7.3.3) the /Parent of every /Page leaf is always a /Pages node,
    /// so the returned view exposes the full pages API (count, each, add_page, delete_page…).
    [[nodiscard]] pages parent();

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
