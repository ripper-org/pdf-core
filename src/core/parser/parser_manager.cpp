#include "ripper/pdf/core/parser/parser_manager.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/default_cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/default_object_parser.hpp"
#include "ripper/pdf/core/parser/header/header_parser.hpp"
#include "ripper/pdf/core/parser/indirect_object_resolver.hpp"
#include "ripper/pdf/core/parser/object_parser.hpp"
#include "ripper/pdf/core/parser/revision_history/default_revision_history_parser.hpp"
#include "ripper/pdf/core/parser/revision_history/revision_history_parser.hpp"
#include "ripper/pdf/core/parser/trailer/default_trailer_parser.hpp"
#include "ripper/pdf/core/parser/trailer/trailer_parser.hpp"

#include <memory>
#include <utility>

namespace ripper::pdf::core
{
parser_manager::parser_manager(document& doc) : document_{&doc} {}

void parser_manager::set_header_parser(std::unique_ptr<class header_parser> object)
{
    header_parser_ = std::move(object);
}

void parser_manager::set_cross_reference_table_parser(
    std::unique_ptr<class cross_reference_table_parser> object)
{
    xref_parser_ = std::move(object);
}

void parser_manager::set_trailer_parser(std::unique_ptr<class trailer_parser> object)
{
    trailer_parser_ = std::move(object);
}

void parser_manager::set_revision_history_parser(
    std::unique_ptr<class revision_history_parser> object)
{
    revision_parser_ = std::move(object);
}

void parser_manager::set_indirect_object_resolver(
    std::unique_ptr<class indirect_object_resolver> object)
{
    object_resolver_ = std::move(object);
}

void parser_manager::set_object_parser(std::unique_ptr<class object_parser> object)
{
    object_parser_ = std::move(object);
}

void parser_manager::rebind(document& doc) noexcept
{
    document_ = &doc;

    if (header_parser_)
        header_parser_->rebind(doc);

    if (revision_parser_)
        revision_parser_->rebind(doc);

    if (object_resolver_)
        object_resolver_->rebind(doc);
}

header_parser& parser_manager::header_parser()
{
    if (!header_parser_)
        header_parser_ = std::make_unique<class header_parser>(*document_);

    return *header_parser_;
}

cross_reference_table_parser& parser_manager::cross_reference_table_parser()
{
    if (!xref_parser_)
        xref_parser_ = std::make_unique<class default_cross_reference_table_parser>();

    return *xref_parser_;
}

trailer_parser& parser_manager::trailer_parser()
{
    if (!trailer_parser_)
        trailer_parser_ = std::make_unique<class default_trailer_parser>();

    return *trailer_parser_;
}

revision_history_parser& parser_manager::revision_history_parser()
{
    if (!revision_parser_)
        revision_parser_ = std::make_unique<class default_revision_history_parser>(*document_);

    return *revision_parser_;
}

indirect_object_resolver& parser_manager::object_resolver()
{
    if (!object_resolver_)
        object_resolver_ = std::make_unique<class indirect_object_resolver>(*document_);

    return *object_resolver_;
}

object_parser& parser_manager::object_parser()
{
    if (!object_parser_)
        object_parser_ = std::make_unique<class default_object_parser>();

    return *object_parser_;
}
} // namespace ripper::pdf::core
