#include "ripper/pdf/core/document/catalog/pages/page/resource/page_resources.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

namespace ripper::pdf::core
{
page_resources::page_resources(object& resources, indirect_object& owner) noexcept
    : direct_object_view(resources, owner)
{
}

page_resources page_resources::ensure(page& owner)
{
    auto* dict = owner.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto* entry = dict->get("Resources");
    if (entry == nullptr || entry->is_null())
    {
        dict->set("Resources", object{dictionary_object{}});
        entry = dict->get("Resources");
    }

    return page_resources{*entry, owner.obj()};
}

bool page_resources::empty() const
{
    const auto* resources = resources_dictionary();

    return resources == nullptr || resources->empty();
}

font_resources page_resources::fonts()
{
    return font_resources::ensure(*this);
}

const dictionary_object* page_resources::resources_dictionary() const
{
    const auto& entry = value();
    if (entry.is_null())
        return nullptr;

    if (const auto* resources = entry.as_dictionary())
        return resources;

    const auto* resolved = document().resolve_object_deep(&entry);
    if (resolved != nullptr && resolved->is_dictionary())
        return resolved->as_dictionary();

    throw logic_exception{"Page /Resources must be a dictionary"};
}
} // namespace ripper::pdf::core
