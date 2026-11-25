#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
page_contents::page_contents(indirect_object& page_obj) noexcept : object_view(page_obj) {}

bool page_contents::empty() const
{
    const auto* entry = contents();
    if (entry == nullptr || entry->is_null())
        return true;

    const auto* arr = entry->as_array();

    return arr != nullptr && arr->empty();
}

bool page_contents::is_array() const
{
    const auto* entry = contents();

    return entry != nullptr && entry->is_array();
}

std::size_t page_contents::size() const
{
    const auto* entry = contents();
    if (entry == nullptr || entry->is_null())
        return 0;

    if (entry->as_indirect_reference() != nullptr)
        return 1;

    if (const auto* arr = entry->as_array())
        return arr->size();

    throw logic_exception{
        "Page /Contents must be a stream reference or an array of stream references"};
}

content_stream page_contents::at(std::size_t index) const
{
    const auto& ref = reference_at(index);

    auto& doc = obj().identity().owner();
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

    auto& dict = dictionary();
    auto* entry = dict.get("Contents");

    // Absent or null /Contents: a lone reference is the canonical single form.
    if (entry == nullptr)
    {
        dict.set("Contents", object{ref});
        return;
    }

    if (entry->is_null())
    {
        *entry = object{ref};
        return;
    }

    if (auto* arr = entry->as_array())
    {
        arr->push_back(object{ref});
        return;
    }

    if (const auto* existing = entry->as_indirect_reference())
    {
        array_object promoted;
        promoted.push_back(object{*existing});
        promoted.push_back(object{ref});
        *entry = object{std::move(promoted)};
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

    auto& dict = dictionary();
    auto* entry = dict.get("Contents");

    if (entry == nullptr || entry->is_null() || index >= size())
        throw std::out_of_range{"Page /Contents index out of range"};

    if (auto* arr = entry->as_array())
    {
        arr->items()[index] = object{ref};
        return;
    }

    if (entry->as_indirect_reference() != nullptr)
    {
        *entry = object{ref};
        return;
    }

    throw logic_exception{
        "Page /Contents must be a stream reference or an array of stream references"};
}

void page_contents::remove(std::size_t index)
{
    auto& dict = dictionary();
    auto* entry = dict.get("Contents");

    if (entry == nullptr || entry->is_null())
        throw std::out_of_range{"Page /Contents is empty"};

    if (entry->as_indirect_reference() != nullptr)
    {
        if (index != 0)
            throw std::out_of_range{"Page /Contents index out of range"};

        *entry = object{};
        return;
    }

    auto* arr = entry->as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    auto& items = arr->items();
    if (index >= items.size())
        throw std::out_of_range{"Page /Contents index out of range"};

    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));

    // Collapse back to the most compact legal form.
    if (items.size() == 1)
        *entry = object{items.front()};
    else if (items.empty())
        *entry = object{};
}

void page_contents::clear()
{
    auto& dict = dictionary();

    auto* entry = dict.get("Contents");
    if (entry == nullptr)
        return;

    *entry = object{};
}

object* page_contents::contents()
{
    auto* dict = obj().content().as_dictionary();
    return dict == nullptr ? nullptr : dict->get("Contents");
}

const object* page_contents::contents() const
{
    const auto* dict = obj().content().as_dictionary();
    return dict == nullptr ? nullptr : dict->get("Contents");
}

dictionary_object& page_contents::dictionary()
{
    auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    return *dict;
}

array_object& page_contents::ensure_array()
{
    auto& dict = dictionary();
    auto* entry = dict.get("Contents");

    if (entry == nullptr)
    {
        dict.set("Contents", object{array_object{}});
        entry = dict.get("Contents");
    }
    else if (entry->is_null())
    {
        *entry = object{array_object{}};
    }
    else if (const auto* existing = entry->as_indirect_reference())
    {
        array_object promoted;
        promoted.push_back(object{*existing});
        *entry = object{std::move(promoted)};
    }

    if (entry == nullptr)
        throw logic_exception{"Failed to create /Contents on the page dictionary"};

    auto* arr = entry->as_array();
    if (arr == nullptr)
        throw logic_exception{
            "Page /Contents must be a stream reference or an array of stream references"};

    return *arr;
}

const indirect_reference& page_contents::reference_at(std::size_t index) const
{
    const auto* entry = contents();
    if (entry == nullptr || entry->is_null())
        throw std::out_of_range{"Page /Contents is empty"};

    if (const auto* ref = entry->as_indirect_reference())
    {
        if (index != 0)
            throw std::out_of_range{"Page /Contents index out of range"};

        return *ref;
    }

    const auto* arr = entry->as_array();
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
