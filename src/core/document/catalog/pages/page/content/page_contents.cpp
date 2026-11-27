#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/helpers/decoded_stream.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
page_contents::page_contents(object& contents, indirect_object& owner) noexcept
    : direct_object_view(contents, owner)
{
}

page_contents page_contents::ensure(page& owner)
{
    auto* dict = owner.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto* entry = dict->get("Contents");
    if (entry == nullptr)
    {
        dict->set("Contents", object{array_object{}});
        entry = dict->get("Contents");
    }

    return page_contents{*entry, owner.obj()};
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
    ensure_array().push_back(object{ref});
}

void page_contents::insert(std::size_t index, content_stream stream)
{
    const auto count = size();
    if (index > count)
        throw std::out_of_range{"Page /Contents index out of range"};

    const auto ref = stream.obj().identity().reference();

    auto& items = ensure_array().items();
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), object{ref});
}

void page_contents::replace(std::size_t index, content_stream stream)
{
    if (index >= size())
        throw std::out_of_range{"Page /Contents index out of range"};

    const auto ref = stream.obj().identity().reference();
    ensure_array().items()[index] = object{ref};
}

void page_contents::remove(std::size_t index)
{
    if (index >= size())
        throw std::out_of_range{"Page /Contents index out of range"};

    auto& items = ensure_array().items();
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
}

void page_contents::clear()
{
    value() = object{array_object{}};
}

void page_contents::squash()
{
    const auto sources = streams();

    if (sources.empty())
        return;

    if (sources.size() == 1)
    {
        value() = object{sources.front().obj().identity().reference()};
        return;
    }

    std::vector<decoded_stream> decoded;
    decoded.reserve(sources.size());

    std::size_t total = 0;
    for (const auto& source : sources)
    {
        const auto* stream_obj = source.stream();
        if (stream_obj == nullptr)
            throw logic_exception{"Page /Contents reference does not point to a stream"};

        auto bytes = stream_obj->decode();
        const auto view = bytes.bytes();
        if (view.size() > std::numeric_limits<std::size_t>::max() - total)
            throw logic_exception{"Squashed content stream is too large"};

        total += view.size();

        decoded.push_back(std::move(bytes));
    }

    stream_object merged;

    auto& output = merged.stream().data();
    output.reserve(total);

    for (auto& bytes : decoded)
    {
        const auto view = bytes.bytes();

        output.insert(output.end(), view.begin(), view.end());
    }

    merged.sync_length();

    auto& merged_obj = object_manager::allocate_stream(document(), std::move(merged));

    value() = object{merged_obj.identity().reference()};
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
