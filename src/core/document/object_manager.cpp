#include "ripper/pdf/core/document/object_manager.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/resource/page_resources.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/parser.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace ripper::pdf::core
{
indirect_object& object_manager::create_object(document& doc, object value)
{
    auto& xref = doc.cross_reference_table();

    auto ref = xref.reserve();
    auto obj = std::make_unique<indirect_object>(object_identity{&doc, ref}, std::move(value));

    auto* raw = xref.commit(std::move(obj));
    if (raw == nullptr)
        throw logic_exception{"Failed to commit object to cross-reference table"};

    return *raw;
}

catalog object_manager::parse_catalog(document& doc)
{
    auto root_ref = doc.trailer().compiled().root();
    if (!root_ref)
        throw parse_exception{"Trailer is missing required /Root reference"};

    return ripper::pdf::core::catalog{*doc.resolve_object(*root_ref)};
}

catalog object_manager::create_catalog(document& doc)
{
    auto& xref = doc.cross_reference_table();
    auto& trl = doc.trailer().active_trailer();

    auto catalog_ref = xref.reserve();
    auto pages_ref = xref.reserve();

    dictionary_object pages_dict;
    pages_dict.set("Type", object{name_object{"Pages"}});
    pages_dict.set("Kids", object{array_object{}});
    pages_dict.set("Count", object{std::int64_t{0}});

    auto pages_obj = std::make_unique<indirect_object>(object_identity{&doc, pages_ref},
                                                       object{std::move(pages_dict)});

    auto* raw_pages = xref.commit(std::move(pages_obj));
    if (raw_pages == nullptr)
        throw logic_exception{"Failed to commit root Pages object to cross-reference table"};

    dictionary_object catalog_dict;
    catalog_dict.set("Type", object{name_object{"Catalog"}});
    catalog_dict.set("Pages", object{pages_ref});

    auto catalog_obj = std::make_unique<indirect_object>(object_identity{&doc, catalog_ref},
                                                         object{std::move(catalog_dict)});

    auto* raw_catalog = xref.commit(std::move(catalog_obj));
    if (raw_catalog == nullptr)
        throw logic_exception{"Failed to commit Catalog (Root) object to cross-reference table"};

    trl.dictionary().set("Root", object{catalog_ref});

    return ripper::pdf::core::catalog{*raw_catalog};
}

page object_manager::create_page(document& doc, pages& parent, const geometry::rect& box)
{
    auto& xref = doc.cross_reference_table();

    const auto page_ref = xref.reserve();

    const auto normalized =
        geometry::rect::from_corners({box.x, box.y}, {box.x + box.width, box.y + box.height});

    // /MediaBox uses the [llx lly urx ury] corner form (ISO 32000-1 §7.9.5).
    array_object mediabox;
    mediabox.push_back(object{normalized.x});
    mediabox.push_back(object{normalized.y});
    mediabox.push_back(object{normalized.x + normalized.width});
    mediabox.push_back(object{normalized.y + normalized.height});

    dictionary_object page_dict;
    page_dict.set("Type", object{name_object{"Page"}});
    page_dict.set("Parent", object{parent.obj().identity().reference()});
    page_dict.set("MediaBox", object{std::move(mediabox)});

    auto page_obj = std::make_unique<indirect_object>(object_identity{&doc, page_ref},
                                                      object{std::move(page_dict)});

    auto* raw_page = xref.commit(std::move(page_obj));
    if (raw_page == nullptr)
        throw logic_exception{"Failed to commit page to cross-reference table"};

    return page{*raw_page};
}

page_contents object_manager::create_page_contents(page& page)
{
    auto* dict = page.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    if (dict->get("Contents") == nullptr)
        dict->set("Contents", object{array_object{}});

    return page_contents{*dict->get("Contents"), page.obj()};
}

page_contents object_manager::parse_page_contents(page& page)
{
    auto* dict = page.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto* entry = dict->get("Contents");
    if (entry == nullptr)
        throw logic_exception{"Page has no /Contents entry"};

    return page_contents{*entry, page.obj()};
}

page_resources object_manager::create_page_resources(page& page)
{
    auto* dict = page.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto* entry = dict->get("Resources");
    if (entry == nullptr || entry->is_null())
        dict->set("Resources", object{dictionary_object{}});

    return page_resources{*dict->get("Resources"), page.obj()};
}

page_resources object_manager::parse_page_resources(page& page)
{
    auto* dict = page.obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto* entry = dict->get("Resources");
    if (entry == nullptr)
        throw logic_exception{"Page has no /Resources entry"};

    return page_resources{*entry, page.obj()};
}

font_resources object_manager::create_font_resources(page_resources& resources)
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

indirect_object& object_manager::create_stream(document& doc, stream_object value)
{
    return create_object(doc, object{std::move(value)});
}

content_stream object_manager::create_content_stream(document& doc)
{
    // An empty stream still needs /Length in its dictionary: the serializer
    // writes the dictionary verbatim.
    stream_object stream_obj;
    stream_obj.sync_length();

    return content_stream{create_stream(doc, std::move(stream_obj))};
}

std::unique_ptr<revision_manager> object_manager::parse_revision_history(const document& doc)
{
    if (!doc.has_parser())
        throw logic_exception{"No parser available"};

    return doc.parser()->revision_history();
}

std::unique_ptr<revision_manager> object_manager::create_revision_history()
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));

    cross_reference_section section{std::move(subsections)};

    trailer t{dictionary_object{}};

    std::deque<revision> revisions;
    revisions.emplace_back(std::move(section), std::move(t));

    return std::make_unique<revision_manager>(std::move(revisions));
}

header object_manager::parse_header(const document& doc)
{
    if (!doc.has_parser())
        throw logic_exception{"No parser available"};

    return doc.parser()->parse_header();
}

header object_manager::create_header()
{
    // TODO: create some mechanism to determine the correct version based on
    // current file features (or just default to latest).
    return ripper::pdf::core::header{"1.4"};
}

} // namespace ripper::pdf::core
