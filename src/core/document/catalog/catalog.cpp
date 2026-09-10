#include "ripper/pdf/core/document/catalog/catalog.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

namespace ripper::pdf::core
{
catalog::catalog(indirect_object& obj) noexcept : object_view(obj) {}

class pages catalog::pages()
{
    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw parse_exception{"Catalog content is not a dictionary"};

    auto pages_ref = d->get_indirect_reference("Pages");
    if (pages_ref == nullptr)
        throw parse_exception{"Catalog is missing required /Pages reference"};

    auto* resolved = obj().identity().owner().resolve_object(*pages_ref);

    return ripper::pdf::core::pages{*resolved};
}

indirect_reference catalog::root_pages_indirect_reference()
{
    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw parse_exception{"Catalog content is not a dictionary"};

    const auto* pages_ref = d->get_indirect_reference("Pages");
    if (pages_ref == nullptr)
        throw parse_exception{"Catalog is missing required /Pages reference"};

    return *pages_ref;
}
} // namespace ripper::pdf::core
