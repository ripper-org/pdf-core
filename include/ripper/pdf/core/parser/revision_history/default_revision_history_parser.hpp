#pragma once

#include "ripper/io/core/reader/reader.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/revision_history/revision_history_parser.hpp"
#include "ripper/pdf/core/parser/trailer/trailer_parser.hpp"

#include <memory>
#include <optional>

namespace ripper::pdf::core
{
class document;
class trailer;

class default_revision_history_parser : public revision_history_parser
{
public:
    explicit default_revision_history_parser(document& document);

    default_revision_history_parser(document& document,
                                    std::unique_ptr<class cross_reference_table_parser> xref_parser,
                                    std::unique_ptr<class trailer_parser> trailer_parser);

    [[nodiscard]] std::unique_ptr<revision_manager> parse() override;

    /// Rebind this parser and its sub-parsers to `document`.
    void rebind(document& document) noexcept override;

private:
    document* _document;

    std::unique_ptr<class cross_reference_table_parser> _xref_parser;
    std::unique_ptr<class trailer_parser> _trailer_parser;

    [[nodiscard]] std::optional<std::size_t>
    find_start_xref_offset(ripper::io::core::reader& reader);
    [[nodiscard]] std::optional<std::size_t> extract_prev_offset(const class trailer& trailer);
};
} // namespace ripper::pdf::core
