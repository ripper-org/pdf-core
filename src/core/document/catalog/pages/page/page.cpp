#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

namespace ripper::pdf::core
{
page::page(indirect_object& obj) noexcept : object_view(obj) {}

std::optional<class pages> page::parent()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return std::nullopt;

    const auto* parent_ref = d->get_indirect_reference("Parent");
    if (parent_ref == nullptr)
        return std::nullopt;

    auto* resolved = obj().identity().owner().resolve_object(*parent_ref);
    if (resolved == nullptr)
        return std::nullopt;

    const auto* parent_dict = resolved->content().as_dictionary();
    if (parent_dict == nullptr)
        throw logic_exception{"Page /Parent is not a dictionary"};

    const auto* type = parent_dict->get_name("Type");
    if (type == nullptr || type->value != "Pages")
        throw logic_exception{"Page /Parent does not have /Type /Pages"};

    return pages{*resolved};
}
} // namespace ripper::pdf::core
