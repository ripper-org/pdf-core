#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <string>
#include <utility>

namespace ripper::pdf::core
{
page::page(indirect_object& obj) noexcept : object_view(obj) {}

pages page::parent()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page object view object is not a dictionary"};

    const auto* parent_ref = d->get_indirect_reference("Parent");
    if (parent_ref == nullptr)
        throw logic_exception{"Page /Parent is not an indirect referece"};

    auto* resolved = obj().identity().owner().resolve_object(*parent_ref);
    if (resolved == nullptr)
        throw logic_exception{"Could not resolve Page /Parent indirect refernce"};

    const auto* parent_dict = resolved->content().as_dictionary();
    if (parent_dict == nullptr)
        throw logic_exception{"Page /Parent is not a dictionary"};

    const auto* type = parent_dict->get_name("Type");
    if (type == nullptr || type->value != "Pages")
        throw logic_exception{"Page /Parent object does not have /Type /Pages"};

    return pages{*resolved};
}

page_contents page::content()
{
    auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page content is not a dictionary"};

    auto& doc = obj().identity().owner();

    // A fresh page has no /Contents entry yet; allocate one on first access so
    // callers can always write to the page's content.
    if (dict->get("Contents") == nullptr)
        return object_manager::create_page_contents(doc, *this);

    return object_manager::parse_page_contents(*this);
}

canvas page::canvas(canvas_mode mode)
{
    rebind_to_active_revision();

    page_contents contents = content();

    auto streams = contents.streams();
    if (streams.empty())
        throw logic_exception{"Page has no content stream to draw on"};

    auto* stream = streams.back().stream();
    if (stream == nullptr)
        throw logic_exception{"Page /Contents does not reference a stream"};

    return ripper::pdf::core::canvas{*stream};
}
} // namespace ripper::pdf::core
