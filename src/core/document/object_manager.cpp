#include "ripper/pdf/core/document/object_manager.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
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

page_contents object_manager::create_page_contents(document& doc, page& page)
{
    // The page may still be bound to an older revision, so rebind before taking
    // any reference into its content: rebinding invalidates earlier pointers.
    page.rebind_to_active_revision();

    if (page.obj().content().as_dictionary() == nullptr)
        throw logic_exception{"Page object is not a dictionary"};

    auto& xref = doc.cross_reference_table();

    // An empty stream still needs /Length in its dictionary: the serializer
    // writes the dictionary verbatim.
    stream_object stream_obj;
    stream_obj.sync_length();

    auto content_stream_ref = xref.reserve();
    auto content_stream_obj = std::make_unique<indirect_object>(
        object_identity{&doc, content_stream_ref}, object{std::move(stream_obj)});

    auto* raw_content_stream = xref.commit(std::move(content_stream_obj));
    if (raw_content_stream == nullptr)
        throw logic_exception{"Failed to commit content stream to cross-reference table"};

    page_contents contents{page.obj()};

    // "Create" replaces any existing /Contents rather than extending it.
    contents.clear();
    contents.append(content_stream{*raw_content_stream});

    return contents;
}

page_contents object_manager::parse_page_contents(page& page)
{
    // Nothing is resolved here: /Contents is validated lazily by the view, so
    // reading a page's contents does not load every referenced stream.
    return page_contents{page.obj()};
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
    return ripper::pdf::core::header{"1.4"};
}

} // namespace ripper::pdf::core
