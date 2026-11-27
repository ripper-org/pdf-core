#pragma once

#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"

#include <memory>

namespace ripper::pdf::core
{
class document;

/// Document-level object allocation and bootstrap utilities.
///
/// This type owns the primitives that cannot belong to any single typed view:
/// allocating a new indirect object (`allocate_object` / `allocate_stream`) and
/// bootstrapping the document structures that are not object views (header and
/// revision history).
///
/// Typed views (`catalog`, `pages`, `page_contents`, …) construct themselves
/// through their own `parse` / `create` / `ensure` methods and call the
/// allocator here when a new indirect object is required.
///
/// ## Responsibilities
///
/// - **Allocation**: reserve a reference, commit the object to the active
///   revision, and return a stable reference to it.
///
/// - **Structural bootstrap**: build the initial header and revision history for
///   new documents, or parse them from file.
///
/// ## Usage
///
/// ```cpp
/// auto& obj = object_manager::allocate_object(doc, object{...});
/// ```
class object_manager
{
public:
    object_manager() = delete;

    /// Allocate a new indirect object and commit it to the active revision.
    ///
    /// The returned reference remains valid for the lifetime of the document.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static indirect_object& allocate_object(document& doc, object value);

    /// Allocate a new indirect stream object and commit it to the active revision.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static indirect_object& allocate_stream(document& doc, stream_object value);

    /// Parse the revision history from file.
    ///
    /// Delegates to the parser to reconstruct the xref and trailer from the input
    /// and returns the assembled revision history without caching.
    ///
    /// @throws parse_exception if the parser fails.
    [[nodiscard]] static std::unique_ptr<revision_manager>
    parse_revision_history(const document& doc);

    /// Generate a new revision history with default values.
    ///
    /// Constructs initial xref and trailer entries suitable for a new document,
    /// including the required xref entry 0 with object number 0 and generation 65535.
    [[nodiscard]] static std::unique_ptr<revision_manager> create_revision_history();

    /// Parse the PDF header from file without caching.
    ///
    /// Delegates to the parser to extract the header marker and version.
    ///
    /// @throws parse_exception if the parser fails or no parser is available.
    [[nodiscard]] static class header parse_header(const document& doc);

    /// Generate a new PDF header with default values.
    ///
    /// Returns a header object set to PDF 1.4 (suitable for most documents).
    [[nodiscard]] static class header create_header();
};

} // namespace ripper::pdf::core
