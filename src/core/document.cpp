#include "ripper/pdf/core/document.hpp"

#include "core/util/scope_exit.hpp"
#include "ripper/io/core/reader/file_reader.hpp"
#include "ripper/io/core/reader/reader.hpp"
#include "ripper/io/core/writer/file_writer.hpp"
#include "ripper/io/core/writer/writer.hpp"
#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/document/trailer/trailer_manager.hpp"
#include "ripper/pdf/core/document_save_strategy/consolidate_document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/dump_document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/incremental_document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/save_strategy_type.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/parser.hpp"
#include "ripper/pdf/core/serializer/serializer.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

namespace ripper::pdf::core
{
document::document(std::unique_ptr<ripper::io::core::reader> reader,
                   std::unique_ptr<ripper::io::core::writer> writer)
    : reader_(std::move(reader)), writer_(std::move(writer))
{
    if (reader_)
        parser_ = std::make_unique<class parser>(*this);

    if (writer_)
        serializer_ = std::make_unique<class serializer>(*this);
}

document::document(const document& other)
    : reader_{other.reader_ ? other.reader_->clone() : nullptr}, writer_{nullptr},
      serializer_{nullptr}, save_strategy_{nullptr}, header_{other.header_},
      revision_manager_{other.revision_manager_
                            ? std::make_unique<class revision_manager>(*other.revision_manager_)
                            : nullptr},
      limits_{other.limits_}
{
    assert(other.resolving_.empty() && "Cannot copy a document during object resolution");

    if (reader_)
        parser_ = std::make_unique<class parser>(*this);

    rebind_object_owners();
}

document& document::operator=(const document& other)
{
    if (this != &other)
    {
        document copy{other};
        *this = std::move(copy);
    }

    return *this;
}

document::document(document&& other) noexcept
    : reader_{std::move(other.reader_)}, parser_{std::move(other.parser_)},
      writer_{std::move(other.writer_)}, serializer_{std::move(other.serializer_)},
      save_strategy_{std::move(other.save_strategy_)}, header_{std::move(other.header_)},
      revision_manager_{std::move(other.revision_manager_)}, limits_{other.limits_}
{
    assert(other.resolving_.empty() && "Cannot move a document during object resolution");

    other.header_.reset();

    rebind_components();
    rebind_object_owners();
}

document& document::operator=(document&& other) noexcept
{
    if (this == &other)
        return *this;

    // Destroy the destination's previous object graph before adopting the
    // source state; identities in that graph point at this document.
    reader_.reset();
    parser_.reset();
    writer_.reset();
    serializer_.reset();
    save_strategy_.reset();
    header_.reset();
    revision_manager_.reset();
    resolving_.clear();

    reader_ = std::move(other.reader_);
    parser_ = std::move(other.parser_);
    writer_ = std::move(other.writer_);
    serializer_ = std::move(other.serializer_);
    save_strategy_ = std::move(other.save_strategy_);
    header_ = std::move(other.header_);
    revision_manager_ = std::move(other.revision_manager_);
    limits_ = other.limits_;
    other.header_.reset();

    rebind_components();
    rebind_object_owners();

    return *this;
}

document document::open(const std::filesystem::path& path)
{
    return document{std::make_unique<ripper::io::core::file_reader>(path), nullptr};
}

document document::create(const std::filesystem::path& path)
{
    return document{nullptr, std::make_unique<ripper::io::core::file_writer>(path)};
}

void document::save()
{
    if (!save_strategy_)
        save_strategy_ = std::make_unique<consolidate_document_save_strategy>();

    save_strategy_->save(*this);
}

void document::save(save_strategy_type type)
{
    switch (type)
    {
        case save_strategy_type::consolidate:
            save_strategy_ = std::make_unique<consolidate_document_save_strategy>();
            break;
        case save_strategy_type::dump:
            save_strategy_ = std::make_unique<dump_document_save_strategy>();
            break;
        case save_strategy_type::incremental:
            save_strategy_ = std::make_unique<incremental_document_save_strategy>();
            break;
    }

    save();
}

void document::set_save_strategy(std::unique_ptr<class document_save_strategy> strategy)
{
    save_strategy_ = std::move(strategy);
}

void document::set_writer(std::unique_ptr<ripper::io::core::writer> writer)
{
    writer_ = std::move(writer);
    serializer_ = writer_ ? std::make_unique<class serializer>(*this) : nullptr;
}

void document::set_resource_limits(const struct resource_limits& limits) noexcept
{
    limits_ = limits;
}

const resource_limits& document::resource_limits() const noexcept
{
    return limits_;
}

bool document::has_reader() const
{
    return static_cast<bool>(reader_);
}

bool document::has_parser() const
{
    return static_cast<bool>(parser_);
}

bool document::has_writer() const
{
    return static_cast<bool>(writer_);
}

bool document::has_serializer() const
{
    return static_cast<bool>(serializer_);
}

ripper::io::core::reader* document::reader() const
{
    return reader_.get();
}

parser* document::parser() const
{
    return parser_.get();
}

ripper::io::core::writer* document::writer() const
{
    return writer_.get();
}

serializer* document::serializer() const
{
    return serializer_.get();
}

header& document::header()
{
    if (header_.has_value())
        return *header_;

    header_ = has_parser() ? object_manager::parse_header(*this) : object_manager::create_header();

    return *header_;
}

cross_reference_manager& document::cross_reference_table()
{
    return initialize_revision_manager().xref();
}

trailer_manager& document::trailer()
{
    return initialize_revision_manager().trailer();
}

revision_manager& document::revisions()
{
    return initialize_revision_manager();
}

catalog document::catalog()
{
    auto root_ref = trailer().compiled().root();

    if (!root_ref)
        return object_manager::create_catalog(*this);

    auto* entry = cross_reference_table().find(*root_ref);
    if (entry == nullptr)
        throw parse_exception{"Root indirect_object not found in cross-reference table"};

    if (entry->is_resolved())
        return ripper::pdf::core::catalog{*entry->indirect_object()};

    return object_manager::parse_catalog(*this);
}

indirect_object* document::resolve_object(indirect_reference ref)
{
    auto* entry = cross_reference_table().find(ref);
    if (entry == nullptr)
        throw parse_exception{"Object not found in cross-reference table"};

    if (entry->is_resolved())
        return entry->indirect_object();

    if (!has_parser())
        throw logic_exception{"No parser available to resolve unresolved indirect_object"};

    // Guard against recursive resolution. A compressed xref entry can point at
    // an object stream that (directly or transitively) contains the entry
    // itself; without this guard a small malformed document exhausts the stack.
    if (resolving_.contains(ref))
    {
        throw parse_exception{"Recursive resolution of indirect object " +
                              std::to_string(ref.object_number())};
    }

    if (resolving_.size() >= limits_.max_resolution_depth)
        throw parse_exception{"Maximum indirect object resolution depth exceeded"};

    scope_exit guard{[this, ref] { resolving_.erase(ref); }};

    resolving_.insert(ref);

    auto parsed = parser_->parse_object(ref);

    return entry->resolve(std::make_unique<indirect_object>(std::move(parsed)));
}

indirect_object& document::resolve_object_to_active_revision(indirect_reference ref)
{
    auto& xref = cross_reference_table();
    auto& active = xref.active_section();

    if (auto* existing = active.find(ref))
    {
        if (auto* obj = existing->indirect_object())
            return *obj;
    }

    auto* entry = xref.find(ref);
    if (entry == nullptr)
        throw logic_exception{"Object " + std::to_string(ref.object_number()) + " not found"};

    resolve_object(ref);

    auto* cloned_entry = active.add_entry_from(*entry);
    if (cloned_entry == nullptr)
        throw logic_exception{"Failed to clone object " + std::to_string(ref.object_number()) +
                              " to active revision"};

    auto* obj = cloned_entry->indirect_object();
    if (obj == nullptr)
        throw logic_exception{"Cloned object " + std::to_string(ref.object_number()) +
                              " has no indirect object"};

    return *obj;
}

revision& document::create_new_revision()
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));

    cross_reference_section new_section{std::move(subsections)};

    class trailer new_trailer{dictionary_object{}};
    new_trailer.dictionary().set(
        "Size", object{static_cast<std::int64_t>(cross_reference_table().next_object_number())});

    auto& revs = initialize_revision_manager().all();
    if (revs.size() > 1)
    {
        auto& prev = revs[revs.size() - 2];

        auto prev_startxref = prev.section().startxref_offset();
        if (prev_startxref)
            new_trailer.dictionary().set("Prev",
                                         object{static_cast<std::int64_t>(*prev_startxref)});
    }

    revision new_rev{std::move(new_section), std::move(new_trailer)};
    initialize_revision_manager().push(std::move(new_rev));

    return initialize_revision_manager().current();
}

revision_manager& document::initialize_revision_manager()
{
    if (revision_manager_ != nullptr)
        return *revision_manager_;

    revision_manager_ = has_parser() ? object_manager::parse_revision_history(*this)
                                     : object_manager::create_revision_history();

    return *revision_manager_;
}

void document::rebind_components() noexcept
{
    if (parser_)
        parser_->rebind(*this);

    if (serializer_)
        serializer_->rebind(*this);
}

void document::rebind_object_owners() noexcept
{
    if (!revision_manager_)
        return;

    // Walk every revision, including superseded sections. The compiled xref view
    // only exposes newest-wins entries, so it would miss resolved objects that
    // only exist in older revisions.
    for (auto& revision : revision_manager_->all())
    {
        for (auto& subsection : revision.section().subsections())
        {
            for (auto& entry : subsection.entries())
            {
                if (auto* object = entry.second.indirect_object())
                    object->identity().rebind_owner(*this);
            }
        }
    }
}
} // namespace ripper::pdf::core
