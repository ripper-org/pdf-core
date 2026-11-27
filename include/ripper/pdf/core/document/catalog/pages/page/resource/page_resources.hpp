#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/resource/font_resources.hpp"
#include "ripper/pdf/core/document/object/helpers/direct_object_view.hpp"

namespace ripper::pdf::core
{
class dictionary_object;
class indirect_object;
class object;
class page;

/// The /Resources entry of a page (ISO 32000-1 §7.8.3).
///
/// Facet of a `page`: a non-owning view over the page's /Resources *slot*.
/// Resources are grouped by category (/Font, /XObject, /ExtGState, …); this view
/// exposes the categories that are modelled as typed facets. Only fonts are
/// modelled so far, because they are the only category required to draw text.
///
/// Mutation happens in place on the page dictionary. The page must already be
/// bound to the active revision (`rebind_to_active_revision()`), otherwise the
/// change is not captured by an incremental save. This view cannot rebind
/// itself; obtain a fresh one from `page::resources()` after rebinding.
///
/// The owning page's indirect object must outlive the view.
class page_resources : public direct_object_view
{
public:
    /// Returns a view over `owner`'s /Resources slot, materializing an empty
    /// dictionary when the entry is absent or null.
    ///
    /// Does not register any resources and does not resolve inherited
    /// /Resources; callers that need inheritance should copy the effective value
    /// into the page before calling this.
    ///
    /// The owner must already be bound to the active revision, otherwise the
    /// change is not captured by an incremental save.
    ///
    /// @return A non-owning view over the page's /Resources slot; the page must
    ///         outlive it.
    ///
    /// @throws logic_exception if the page object is not a dictionary.
    [[nodiscard]] static page_resources ensure(page& owner);

    /// Returns `true` if /Resources is absent, null or an empty dictionary.
    ///
    /// @throws logic_exception if /Resources is neither a dictionary nor a
    ///         reference to one.
    [[nodiscard]] bool empty() const;

    /// Returns a view over the /Font category.
    ///
    /// Creates an empty /Font dictionary when absent.
    ///
    /// @throws logic_exception if /Resources or /Font is malformed.
    [[nodiscard]] font_resources fonts();

private:
    /// @param resources the page's /Resources slot.
    /// @param owner the page's indirect_object (must hold a dictionary).
    page_resources(object& resources, indirect_object& owner) noexcept;

    /// Returns /Resources as a dictionary, or `nullptr` when absent or null.
    [[nodiscard]] const dictionary_object* resources_dictionary() const;
};
} // namespace ripper::pdf::core
