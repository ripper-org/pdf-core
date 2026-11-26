#pragma once

#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"

namespace ripper::pdf::core
{
class document;

/// Non-owning view over a direct PDF object that lives inside an indirect object.
///
/// Unlike `indirect_object_view`, the wrapped value has no identity in the
/// cross-reference table: it is a dictionary entry or array element belonging
/// to an indirect object. The view is therefore bound to an owner and cannot be
/// rebound to an active revision on its own.
///
/// The view targets the *slot*, not the current value. Replacing the value, for
/// example promoting a single indirect reference to an array, does not
/// invalidate the view; replacing the slot or an ancestor does.
///
/// The owner must outlive the view. A view must not be kept across a rebind of
/// its owner or across any mutation that replaces the viewed slot or one of its
/// ancestors.
class direct_object_view
{
public:
    /// @param value the direct object slot to view.
    /// @param owner the indirect object that contains `value`.
    direct_object_view(object& value, indirect_object& owner) noexcept;

    /// Returns the viewed direct object.
    [[nodiscard]] object& value() noexcept;

    /// Returns the viewed direct object.
    [[nodiscard]] const object& value() const noexcept;

    /// Returns the indirect object that owns the viewed value.
    [[nodiscard]] indirect_object& owner() noexcept;

    /// Returns the indirect object that owns the viewed value.
    [[nodiscard]] const indirect_object& owner() const noexcept;

    /// Returns the document that owns the indirect object.
    [[nodiscard]] document& document() const;

    /// Rebind this view to another direct object and its owner.
    ///
    /// Used by facets that re-locate their slot after rebinding the owner.
    void rebind(object& value, indirect_object& owner) noexcept;

protected:
    object* value_;
    indirect_object* owner_;
};
} // namespace ripper::pdf::core
