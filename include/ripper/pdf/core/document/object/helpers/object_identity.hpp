#pragma once

#include "ripper/pdf/core/document/object/indirect_reference.hpp"

namespace ripper::pdf::core
{
class document;

/// The object identity is a combination of an owned `indirect reference`
/// (object number + generation number) and a non-owning `document` reference.
///
/// All concrete PDF indirect object types (pages, fonts, images, etc.) have
/// a way to access its `object_identity` as a way to allow access to the
/// owning document.
class object_identity
{
public:
    /// Construct an object identity bound to `doc` and identified by `ref`.
    ///
    /// The document pointer must not be null and must outlive this object identity.
    object_identity(document* doc, indirect_reference ref);

    /// Returns the indirect reference (object number + generation number) that
    /// uniquely identifies this object identity within its owning document.
    [[nodiscard]] const indirect_reference& reference() const;

    /// Returns the owning document.
    ///
    /// Derived classes may use this to navigate the document structure or
    /// resolve other indirect references.
    [[nodiscard]] document& owner() const;

    /// Point this identity at `doc`, preserving the indirect reference.
    ///
    /// Used when a document's object graph is moved or deep-copied so existing
    /// objects follow their owner to the destination instance.
    void rebind_owner(document& doc) noexcept;

private:
    document* document_;
    indirect_reference reference_;
};
} // namespace ripper::pdf::core
