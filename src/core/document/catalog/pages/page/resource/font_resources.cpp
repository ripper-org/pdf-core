#include "ripper/pdf/core/document/catalog/pages/page/resource/font_resources.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/resource/page_resources.hpp"
#include "ripper/pdf/core/document/font/font.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ripper::pdf::core
{
font_resources::font_resources(object& fonts, indirect_object& owner) noexcept
    : direct_object_view(fonts, owner)
{
}

font_resources font_resources::ensure(page_resources& resources)
{
    auto& doc = resources.document();
    auto& entry = resources.value();

    if (entry.is_null())
        entry = object{dictionary_object{}};

    auto* resources_dict = entry.as_dictionary();
    if (resources_dict == nullptr && entry.is_indirect_reference())
    {
        const auto* resolved = doc.resolve_object_deep(&entry);
        if (resolved != nullptr && resolved->is_dictionary())
        {
            entry = object{*resolved->as_dictionary()};
            resources_dict = entry.as_dictionary();
        }
    }

    if (resources_dict == nullptr)
        throw logic_exception{"Page /Resources must be a dictionary"};

    auto* font_entry = resources_dict->get("Font");
    if (font_entry == nullptr || font_entry->is_null())
    {
        resources_dict->set("Font", object{dictionary_object{}});
        font_entry = resources_dict->get("Font");
    }

    auto* fonts = font_entry->as_dictionary();
    if (fonts == nullptr && font_entry->is_indirect_reference())
    {
        const auto* resolved = doc.resolve_object_deep(font_entry);
        if (resolved != nullptr && resolved->is_dictionary())
        {
            *font_entry = object{*resolved->as_dictionary()};
            fonts = font_entry->as_dictionary();
        }
    }

    if (fonts == nullptr)
        throw logic_exception{"Page /Font must be a dictionary"};

    return font_resources{*font_entry, resources.owner()};
}

bool font_resources::empty() const
{
    return entries().empty();
}

bool font_resources::has(std::string_view name) const
{
    return entries().contains(std::string{name});
}

const object* font_resources::find(std::string_view name) const
{
    return entries().get(std::string{name});
}

object* font_resources::find(std::string_view name)
{
    return entries().get(std::string{name});
}

std::string font_resources::add(indirect_object& font)
{
    auto& fonts = entries();

    auto name = next_name(fonts);
    fonts.set(name, object{font.identity().reference()});

    return name;
}

void font_resources::set(std::string_view name, indirect_object& font)
{
    entries().set(std::string{name}, object{font.identity().reference()});
}

std::string font_resources::add(font& value)
{
    return add(value.obj());
}

void font_resources::set(std::string_view name, font& value)
{
    set(name, value.obj());
}

void font_resources::remove(std::string_view name)
{
    entries().remove(std::string{name});
}

void font_resources::clear()
{
    entries().entries().clear();
}

const dictionary_object& font_resources::entries() const
{
    const auto* fonts = value().as_dictionary();
    if (fonts == nullptr)
        throw logic_exception{"Page /Font must be a dictionary"};

    return *fonts;
}

dictionary_object& font_resources::entries()
{
    auto* fonts = value().as_dictionary();
    if (fonts == nullptr)
        throw logic_exception{"Page /Font must be a dictionary"};

    return *fonts;
}

std::string font_resources::next_name(const dictionary_object& entries)
{
    for (std::size_t suffix = 1;; ++suffix)
    {
        auto candidate = "F" + std::to_string(suffix);

        if (!entries.contains(candidate))
            return candidate;
    }
}
} // namespace ripper::pdf::core
