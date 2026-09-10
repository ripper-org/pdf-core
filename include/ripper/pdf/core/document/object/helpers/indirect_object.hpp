#pragma once

#include "ripper/pdf/core/document/object/helpers/object_identity.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

namespace ripper::pdf::core
{
/// A PDF indirect object (ISO 32000-1 §7.3.10): an object identifier plus the object value
/// it carries.
///
/// Pairs an `object_identity` (object number + generation, scoped to the owning document) with
/// the carried value (`content()`). The value is arbitrary — any PDF object type — and is
/// accessed via the typed accessors on the carried `object`, e.g. `content().as_dictionary()`.
///
/// `object_identity` knows *which* object this is; `indirect_object` adds the value it carries.
///
/// The typed views (`catalog`, `pages`, `page`, `objstm`, …) are non-owning `object_view`
/// wrappers over an `indirect_object&`, not subclasses of this class.
///
/// `indirect_object` owns its content object; the identity is held by value.
class indirect_object
{
public:
    /// Construct an indirect object from an object identifier and the object value it carries.
    indirect_object(object_identity identity, object content) noexcept;

    /// Returns the `object_identity` identity of this indirect_object, which includes the
    /// owning document and indirect reference.
    [[nodiscard]] const object_identity& identity() const noexcept;

    /// Returns a mutable reference to the `object_identity` identity of this indirect_object.
    [[nodiscard]] object_identity& identity() noexcept;

    /// Returns the object value carried by this indirect object.
    ///
    /// May hold any PDF object type: null, boolean, integer, real, string,
    /// name, array, dictionary, stream, or indirect reference.
    [[nodiscard]] const object& content() const noexcept;

    /// Returns a mutable reference to the carried object value.
    [[nodiscard]] object& content() noexcept;

    /// Create a deep copy of this indirect object.
    ///
    /// The cloned object retains the same identity (document + reference) and
    /// receives a fully independent copy of the content object.
    [[nodiscard]] indirect_object clone() const;

private:
    object_identity identity_;
    object content_;
};
} // namespace ripper::pdf::core
