#include "ripper/pdf/core/document/object_manager.hpp"

#include "ripper/pdf/core/document.hpp"
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
#include <deque>
#include <memory>
#include <utility>

namespace ripper::pdf::core
{
indirect_object& object_manager::allocate_object(document& doc, object value)
{
    auto& xref = doc.cross_reference_table();

    auto ref = xref.reserve();
    auto obj = std::make_unique<indirect_object>(object_identity{&doc, ref}, std::move(value));

    auto* raw = xref.commit(std::move(obj));
    if (raw == nullptr)
        throw logic_exception{"Failed to commit object to cross-reference table"};

    return *raw;
}

indirect_object& object_manager::allocate_stream(document& doc, stream_object value)
{
    return allocate_object(doc, object{std::move(value)});
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
