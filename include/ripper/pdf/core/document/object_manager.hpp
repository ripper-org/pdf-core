#pragma once

#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"

#include <memory>

namespace ripper::pdf::core
{
class document;
class pages;

/// Static utility for parsing and creating PDF document objects.
///
/// This type encapsulates all logic for parsing document objects from file data
/// and creating new objects to be written to file. It centralizes the coordination
/// of low-level operations (parser invocation, xref allocation, object commitment)
/// with domain-level concerns (object type validation, invariant enforcement).
///
/// All methods are static and take a `document&` parameter to access the parser,
/// xref, trailer, and serialization infrastructure.
///
/// ## Responsibilities
///
/// - **Parse operations**: Read an object from file via the parser and resolve it
///   into the xref, with type validation as needed.
///
/// - **Create operations**: Allocate a new indirect object, populate with defaults,
///   commit to xref, and wire cross-references as needed (e.g., set trailer /Root).
///
/// - **Structural initialization**: Build initial document structures (header, xref,
///   trailer) for new documents or parsed documents.
///
/// ## Usage
///
/// ```cpp
/// auto cat = object_manager::create_catalog(doc);
/// ```
class object_manager
{
public:
    object_manager() = delete;

    /// Parse the catalog from the file and resolve it into the xref.
    ///
    /// Looks up the /Root reference in the trailer, resolves the object via the parser,
    /// validates its /Type is /Catalog, and returns a typed view.
    ///
    /// @throws parse_exception if the trailer /Root is missing or the resolved object
    ///         is not a valid catalog dictionary.
    [[nodiscard]] static catalog parse_catalog(document& doc);

    /// Allocate a new catalog into the xref and set the trailer /Root.
    ///
    /// Reserves an object reference, constructs a catalog dictionary_object with /Type /Catalog,
    /// commits it to the xref, and wires the trailer /Root to point to it.
    ///
    /// @throws logic_exception if the xref or trailer operations fail.
    [[nodiscard]] static catalog create_catalog(document& doc);

    /// Allocate a new /Page object parented at `parent`.
    ///
    /// Reserves an object reference, builds a page dictionary with
    /// `/Type /Page`, `/Parent`, and a normalized `/MediaBox`, commits it to the
    /// xref, and returns a typed view.
    ///
    /// The page is not added to `parent`'s /Kids; the caller is responsible for
    /// inserting the reference and updating /Count.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static page create_page(document& doc, pages& parent, const geometry::rect& box);

    /// Allocate an empty content stream and attach it to a page's /Contents.
    ///
    /// Rebinds `page` to the active revision (so the modification is captured by
    /// an incremental save), allocates a new content stream with `/Length 0` in
    /// the xref, and sets the page's /Contents to that stream's reference.
    ///
    /// Any existing /Contents is replaced; streams referenced only by the old
    /// value become unreferenced and are not removed from the xref.
    ///
    /// @return A view over the page's /Contents entry.
    ///
    /// @throws logic_exception if the page object is not a dictionary or the
    ///         xref operations fail.
    [[nodiscard]] static page_contents create_page_contents(document& doc, page& page);

    /// Wrap a page's /Contents entry into a lazily validated view.
    ///
    /// Does not resolve anything: the returned view validates and resolves
    /// /Contents on use (`size()`, `at()`, `streams()`), so merely accessing a
    /// page's contents never loads the referenced streams.
    ///
    /// Accepts absent, null, a single indirect stream reference, or an array of
    /// indirect stream references (ISO 32000-1 §7.7.3.3).
    ///
    /// @return A non-owning view over `page`; the page must outlive it.
    [[nodiscard]] static page_contents parse_page_contents(page& page);

    /// Allocate a new, empty content stream in the xref.
    ///
    /// The stream gets `/Length 0` and is committed to the active revision but
    /// is not referenced by any page yet; attach it with `page_contents::append`,
    /// `insert`, or `replace`.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static content_stream create_content_stream(document& doc);

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
