#include "ripper/pdf/core/document/catalog/catalog.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

namespace ripper::pdf::core
{
catalog::catalog(indirect_object& obj) noexcept : indirect_object_view(obj) {}

catalog catalog::parse(document& doc)
{
    auto root_ref = doc.trailer().compiled().root();
    if (!root_ref)
        throw parse_exception{"Trailer is missing required /Root reference"};

    return ripper::pdf::core::catalog{*doc.resolve_object(*root_ref)};
}

catalog catalog::create(document& doc)
{
    auto root_pages = pages::create_root(doc);

    dictionary_object catalog_dict;
    catalog_dict.set("Type", object{name_object{"Catalog"}});
    catalog_dict.set("Pages", object{root_pages.obj().identity().reference()});

    auto& catalog_obj = object_manager::allocate_object(doc, object{std::move(catalog_dict)});

    doc.trailer().active_trailer().dictionary().set("Root",
                                                    object{catalog_obj.identity().reference()});

    return ripper::pdf::core::catalog{catalog_obj};
}

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
