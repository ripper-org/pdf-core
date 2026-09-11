#pragma once

#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"

#include <memory>

namespace ripper::pdf::core
{
class document;
class parser_manager;
class indirect_reference;

/// High-level PDF parser facade for a single `document`.
///
/// This type orchestrates parsing by delegating to components managed by
/// `parser_manager`, and throws on failures.
class parser
{
public:
    /// Construct a parser bound to `doc`.
    ///
    /// The parser stores a reference and does not take ownership of the document.
    /// Backend availability is validated by parse operations.
    explicit parser(document& doc);

    /// Destroy the parser and its internal manager.
    ~parser();

    /// Return the parser manager used by this parser.
    ///
    /// Can be used to replace parser subcomponents.
    [[nodiscard]] parser_manager& manager();

    /// Parse and return the document header.
    [[nodiscard]] header parse_header();

    /// Parse and return the complete revision history of the document.
    [[nodiscard]] std::unique_ptr<revision_manager> revision_history();

    /// Parse any indirect object by reference and return the fully resolved `indirect_object`.
    ///
    /// The result can be cast to a typed subclass (catalog, pages, etc.) when the
    /// caller knows the `/Type` of the object.
    [[nodiscard]] indirect_object parse_object(indirect_reference ref, bool preload_stream = true);

private:
    document& document_;
    std::unique_ptr<parser_manager> manager_;
};
} // namespace ripper::pdf::core
