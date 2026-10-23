#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"

#include <cassert>

namespace ripper::pdf::core
{
object_identity::object_identity(document* doc, indirect_reference ref)
    : document_{doc}, reference_{ref}
{
    assert(doc != nullptr && "object_identity requires a non-null document pointer");
}

const indirect_reference& object_identity::reference() const
{
    return reference_;
}

document& object_identity::owner() const
{
    assert(document_ != nullptr && "object_identity has null document pointer");
    return *document_;
}

void object_identity::rebind_owner(document& doc) noexcept
{
    document_ = &doc;
}
} // namespace ripper::pdf::core
