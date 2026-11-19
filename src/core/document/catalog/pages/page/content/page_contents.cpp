#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <vector>

namespace ripper::pdf::core
{
page_contents::page_contents(document& doc, object& object) noexcept : doc_(doc), object_(object) {}

bool page_contents::empty() const noexcept
{
    if (object_.is_null())
    {
        return true;
    }

    const auto arr = object_.as_array();

    return arr != nullptr && arr->empty();
}

bool page_contents::is_array() const noexcept
{
    return object_.is_array();
}

std::vector<content_stream> page_contents::streams() const
{
    const auto resolve = [this](const indirect_reference& ref) -> content_stream
    {
        auto* resolved = doc_.resolve_object(ref);
        if (resolved == nullptr)
            throw logic_exception{"Page /Contents reference cannot be resolved"};

        if (resolved->content().as_stream() == nullptr)
            throw logic_exception{"Page /Contents reference does not point to a stream"};

        return content_stream{*resolved};
    };

    if (object_.is_null())
    {
        return {};
    }

    if (const auto* ref = object_.as_indirect_reference())
    {
        return {resolve(*ref)};
    }

    const auto* arr = object_.as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    std::vector<content_stream> resolved;
    resolved.reserve(arr->size());

    for (const auto& item : *arr)
    {
        const auto* ref = item.as_indirect_reference();
        if (ref == nullptr)
            throw logic_exception{
                "Page /Contents array entries must be indirect stream references"};

        resolved.push_back(resolve(*ref));
    }

    return resolved;
}

} // namespace ripper::pdf::core
