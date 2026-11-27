#pragma once

#include "ripper/pdf/core/document/object/helpers/direct_object_view.hpp"

#include <string>
#include <string_view>

namespace ripper::pdf::core
{
class dictionary_object;
class indirect_object;
class object;
class page_resources;

/// The /Font resource dictionary of a page (ISO 32000-1 §7.8.3).
///
/// Facet of `page_resources`: a non-owning view over the page's `/Font`
/// dictionary, which maps resource names such as `/F1` to font dictionaries.
/// This is the only resource category required to draw text: the `Tf` operator
/// selects a font by the name registered here.
///
/// New fonts are registered under generated names (`/F1`, `/F2`, …), skipping
/// names that are already present.
///
/// Mutation happens in place on the page dictionary. The page must already be
/// bound to the active revision (`rebind_to_active_revision()`), otherwise the
/// change is not captured by an incremental save. This view cannot rebind
/// itself; obtain a fresh one from `page_resources::fonts()` after rebinding.
///
/// The owning page's indirect object must outlive the view.
class font_resources : public direct_object_view
{
public:
    /// Ensures a page's /Resources and /Font entries are direct dictionaries and
    /// returns a view over /Font.
    ///
    /// Creates missing or null entries and copies indirect values into place.
    /// Only slot normalization happens here; registering individual fonts is
    /// `add()`/`set()`.
    ///
    /// The owning page must already be bound to the active revision, otherwise
    /// the change is not captured by an incremental save.
    ///
    /// @return A non-owning view over the page's /Font slot; the page must
    ///         outlive it.
    ///
    /// @throws logic_exception if /Resources or /Font is not a dictionary.
    [[nodiscard]] static font_resources ensure(page_resources& resources);

    /// Returns `true` if no fonts are registered.
    [[nodiscard]] bool empty() const;

    /// Returns `true` if a font is registered under `name`.
    [[nodiscard]] bool has(std::string_view name) const;

    /// Returns the entry registered as `name`, or `nullptr` if it is absent.
    [[nodiscard]] const object* find(std::string_view name) const;
    [[nodiscard]] object* find(std::string_view name);

    /// Registers `font` under a generated unique name and returns that name.
    [[nodiscard]] std::string add(indirect_object& font);

    /// Registers or overwrites `name` with `font`.
    void set(std::string_view name, indirect_object& font);

    /// Removes `name`; absent names are a no-op.
    void remove(std::string_view name);

    /// Removes every font.
    void clear();

private:
    /// @param fonts the page's `/Font` slot (must hold a dictionary).
    /// @param owner the page's indirect_object.
    font_resources(object& fonts, indirect_object& owner) noexcept;

    /// Returns the `/Font` dictionary, validating its shape.
    [[nodiscard]] const dictionary_object& entries() const;
    [[nodiscard]] dictionary_object& entries();

    /// Returns the smallest `/F<n>` name not present in `entries`.
    [[nodiscard]] static std::string next_name(const dictionary_object& entries);
};
} // namespace ripper::pdf::core
