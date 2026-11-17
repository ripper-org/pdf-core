#pragma once

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
    /// view, with is_array() reporting which form the file used. Returns an
    /// empty view when /Contents is absent or null.
    ///
    /// @throws logic_exception if /Contents is malformed or a referenced object
    ///         is not a stream.
    [[nodiscard]] page_contents content();
};
} // namespace ripper::pdf::core
