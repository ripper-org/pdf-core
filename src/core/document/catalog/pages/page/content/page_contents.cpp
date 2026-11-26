#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
/// Returns the page's /Contents slot, creating a null entry when absent.
[[nodiscard]] object& ensure_contents_slot(indirect_object& page_obj)
{
    auto* dict = page_obj.content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    if (auto* entry = dict->get("Contents"))
        return *entry;

    dict->set("Contents", object{});

    return *dict->get("Contents");
}
} // namespace

page_contents::page_contents(object& contents, indirect_object& page_obj) noexcept
    : direct_object_view(contents, page_obj)
{
}

page_contents::page_contents(indirect_object& page_obj)
    : page_contents{ensure_contents_slot(page_obj), page_obj}
{
}

bool page_contents::empty() const
{
    const auto& entry = value();
    if (entry.is_null())
        return true;

    const auto* arr = entry.as_array();

    return arr != nullptr && arr->empty();
}

bool page_contents::is_array() const
{
    return value().is_array();
}

std::size_t page_contents::size() const
{
    const auto& entry = value();
    if (entry.is_null())
        return 0;

    if (entry.as_indirect_reference() != nullptr)
        return 1;

    if (const auto* arr = entry.as_array())
        return arr->size();

    throw logic_exception{
        "Page /Contents must be a stream reference or an array of stream references"};
}

content_stream page_contents::at(std::size_t index) const
{
    const auto& ref = reference_at(index);

    auto& doc = document();
    auto* resolved = doc.resolve_object(ref);
    if (resolved == nullptr || resolved->content().as_stream() == nullptr)
        throw logic_exception{"Page /Contents reference does not point to a stream"};

    return content_stream{*resolved};
}

std::vector<content_stream> page_contents::streams() const
{
    const auto count = size();

    std::vector<content_stream> resolved;
    resolved.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
        resolved.push_back(at(i));

    return resolved;
}

void page_contents::append(content_stream stream)
{
    const auto ref = stream.obj().identity().reference();

    auto& entry = value();

    // Null /Contents: a lone reference is the canonical single form.
    if (entry.is_null())
    {
        entry = object{ref};
        return;
    }

    if (auto* arr = entry.as_array())
    {
        arr->push_back(object{ref});
        return;
    }

    if (const auto* existing = entry.as_indirect_reference())
    {
        array_object promoted;
        promoted.push_back(object{*existing});
        promoted.push_back(object{ref});
        entry = object{std::move(promoted)};
        return;
    }

    throw logic_exception{
        "Page /Contents must be a stream reference or an array of stream references"};
}

void page_contents::insert(std::size_t index, content_stream stream)
{
    const auto count = size();
    if (index > count)
        throw std::out_of_range{"Page /Contents index out of range"};

    if (index == count)
    {
        append(std::move(stream));
        return;
    }

    const auto ref = stream.obj().identity().reference();

    auto& arr = ensure_array();
    auto& items = arr.items();
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), object{ref});
}

void page_contents::replace(std::size_t index, content_stream stream)
{
    const auto ref = stream.obj().identity().reference();

    auto& entry = value();

    if (entry.is_null() || index >= size())
        throw std::out_of_range{"Page /Contents index out of range"};

    if (auto* arr = entry.as_array())
    {
        arr->items()[index] = object{ref};
        return;
    }

    if (entry.as_indirect_reference() != nullptr)
    {
        entry = object{ref};
        return;
    }

    throw logic_exception{
        "Page /Contents must be a stream reference or an array of stream references"};
}

void page_contents::remove(std::size_t index)
{
    auto& entry = value();

    if (entry.is_null())
        throw std::out_of_range{"Page /Contents is empty"};

    if (entry.as_indirect_reference() != nullptr)
    {
        if (index != 0)
            throw std::out_of_range{"Page /Contents index out of range"};

        entry = object{};
        return;
    }

    auto* arr = entry.as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    auto& items = arr->items();
    if (index >= items.size())
        throw std::out_of_range{"Page /Contents index out of range"};

    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));

    // Collapse back to the most compact legal form.
    if (items.size() == 1)
        entry = object{items.front()};
    else if (items.empty())
        entry = object{};
}

void page_contents::clear()
{
    value() = object{};
}

array_object& page_contents::ensure_array()
{
    auto& entry = value();

    if (entry.is_null())
    {
        entry = object{array_object{}};
    }
    else if (const auto* existing = entry.as_indirect_reference())
    {
        array_object promoted;
        promoted.push_back(object{*existing});
        entry = object{std::move(promoted)};
    }

    auto* arr = entry.as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    return *arr;
}

const indirect_reference& page_contents::reference_at(std::size_t index) const
{
    const auto& entry = value();
    if (entry.is_null())
        throw std::out_of_range{"Page /Contents is empty"};

    if (const auto* ref = entry.as_indirect_reference())
    {
        if (index != 0)
            throw std::out_of_range{"Page /Contents index out of range"};

        return *ref;
    }

    const auto* arr = entry.as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    if (index >= arr->size())
        throw std::out_of_range{"Page /Contents index out of range"};

    const auto* ref = arr->at(index).as_indirect_reference();
    if (ref == nullptr)
        throw logic_exception{"Page /Contents array entries must be indirect stream references"};

    return *ref;
}

} // namespace ripper::pdf::core
