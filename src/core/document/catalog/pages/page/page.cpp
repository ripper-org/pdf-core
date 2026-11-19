#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
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

    const auto* contents = dict->get("Contents");

    // If a page nas no /Contents value set, this means the page is fresh, and we need to
    // manually allocate a new empty stream to allow users to seamlessly access it's content.
    if (contents == nullptr)
    {
    }

    //     auto& doc = obj().identity().owner();
    //
    //     const auto resolve = [&](const indirect_reference& ref) -> content_stream
    //     {
    //         auto* resolved = doc.resolve_object(ref);
    //         if (resolved == nullptr)
    //             throw logic_exception{"Page /Contents reference cannot be resolved"};
    //
    //         if (resolved->content().as_stream() == nullptr)
    //             throw logic_exception{"Page /Contents reference does not point to a stream"};
    //
    //         return content_stream{*resolved};
    //     };
    //
    //     if (const auto* ref = contents->as_indirect_reference())
    //         return page_contents{{resolve(*ref)}};
    //
    //     const auto* array = contents->as_array();
    //     if (array == nullptr)
    //         throw logic_exception{"Page /Contents must be a stream or an array of streams"};
    //
    //     std::vector<content_stream> streams;
    //     streams.reserve(array->size());
    //
    //     for (std::size_t i = 0; i < array->size(); ++i)
    //     {
    //         const auto* ref = array->at(i).as_indirect_reference();
    //         if (ref == nullptr)
    //             throw logic_exception{
    //                 "Page /Contents array entries must be indirect stream references"};
    //
    //         streams.push_back(resolve(*ref));
    //     }
    //
    //     return page_contents{std::move(streams)};
}

canvas page::canvas(canvas_mode mode)
{
    page_contents contents = content();
    if (contents.empty())
    {
    }
    // stream_object* stream = nullptr;
}
} // namespace ripper::pdf::core
