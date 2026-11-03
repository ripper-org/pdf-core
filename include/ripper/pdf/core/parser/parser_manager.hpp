#pragma once

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/header/header_parser.hpp"
#include "ripper/pdf/core/parser/indirect_object_resolver.hpp"
#include "ripper/pdf/core/parser/object_parser.hpp"
#include "ripper/pdf/core/parser/revision_history/revision_history_parser.hpp"
#include "ripper/pdf/core/parser/trailer/trailer_parser.hpp"

#include <memory>

namespace ripper::pdf::core
{

/// Owns and exposes the parser subcomponents used to process a `document`.
///
/// This type centralizes parser dependencies and enables runtime injection
/// of concrete parser implementations (useful for composition and testing).
/// All injected components are owned via `std::unique_ptr`.
class parser_manager
{
public:
    /// Construct a manager bound to `doc`.
    ///
    /// The manager stores a reference and does not take ownership of the document.
    explicit parser_manager(class document& doc);

    /// Replace the header parser implementation.
    void set_header_parser(std::unique_ptr<class header_parser> object);

    /// Replace the cross-reference-table parser implementation.
    void
    set_cross_reference_table_parser(std::unique_ptr<class cross_reference_table_parser> object);

    /// Replace the trailer parser implementation.
    void set_trailer_parser(std::unique_ptr<class trailer_parser> object);

    /// Replace the revision-history parser implementation.
    void set_revision_history_parser(std::unique_ptr<class revision_history_parser> object);

    /// Replace the indirect-indirect_object resolver implementation.
    void set_indirect_object_resolver(std::unique_ptr<class indirect_object_resolver> object);

    /// Replace the indirect_object parser implementation.
    void set_object_parser(std::unique_ptr<class object_parser> object);

    /// Rebind this manager and all constructed subcomponents to `doc`.
    ///
    /// Subcomponents that do not store a document reference are unaffected.
    /// Already-constructed lazy subcomponents are retargeted; subcomponents
    /// created later bind to `doc` at construction time.
    void rebind(document& doc) noexcept;

    /// Access the configured header parser.
    [[nodiscard]] class header_parser& header_parser();

    /// Access the configured cross-reference-table parser.
    [[nodiscard]] class cross_reference_table_parser& cross_reference_table_parser();

    /// Access the configured trailer parser.
    [[nodiscard]] class trailer_parser& trailer_parser();

    /// Access the configured revision-history parser.
    [[nodiscard]] class revision_history_parser& revision_history_parser();

    /// Access the configured indirect-indirect_object resolver.
    [[nodiscard]] class indirect_object_resolver& object_resolver();

    /// Access the configured indirect_object parser.
    [[nodiscard]] class object_parser& object_parser();

private:
    document* document_;

    std::unique_ptr<class header_parser> header_parser_;
    std::unique_ptr<class cross_reference_table_parser> xref_parser_;
    std::unique_ptr<class trailer_parser> trailer_parser_;
    std::unique_ptr<class revision_history_parser> revision_parser_;
    std::unique_ptr<class indirect_object_resolver> object_resolver_;
    std::unique_ptr<class object_parser> object_parser_;
};
} // namespace ripper::pdf::core
