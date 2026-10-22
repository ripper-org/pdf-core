#pragma once

#include "ripper/io/core/reader/reader.hpp"
#include "ripper/io/core/writer/writer.hpp"
#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/header.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer_manager.hpp"
#include "ripper/pdf/core/document_save_strategy/document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/save_strategy_type.hpp"
#include "ripper/pdf/core/parser/parser.hpp"
#include "ripper/pdf/core/serializer/serializer.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <filesystem>
#include <memory>
#include <unordered_set>

namespace ripper::pdf::core
{
/// High-level PDF document facade and primary library entrypoint.
///
/// This type represents a PDF document and centralizes all interactions:
/// opening/creating files, parsing document structures, and exposing
/// materialized document components through a lazy API.
class document
{
public:
    /// Construct a document from optional reader/writer backends.
    ///
    /// The document takes ownership of the provided backends.
    ///
    /// When a reader is present, accessing document objects (xref, catalog, pages, etc.)
    /// will lazily parse the metadata of the document structure reading bytes from `reader`.
    /// These components are cached after first access, so subsequent accesses do not require
    /// additional parsing.
    ///
    /// When a writer is present, the document can be saved through the `save` method and
    /// written to the backing storage through `writer`. All changes made in memory to objects
    /// returned by the document API are reflected in the output of `save`.
    ///
    /// If a document contains a `writer`, but has no `reader`, all objects are created from
    /// scratch and are not based on any existing file content. This can be used to create new
    /// PDF documents.
    ///
    /// If a document contains a `reader`, but has no `writer`, all objects are read from the
    /// input file and cannot be modified or saved. This can be used to parse and inspect
    /// existing PDF documents.
    explicit document(std::unique_ptr<ripper::io::core::reader> reader,
                      std::unique_ptr<ripper::io::core::writer> writer);

    /// Documents own objects that store a back-reference to their document.
    ///
    /// Moving or copying the facade would leave those back-references pointing
    /// at the original (or a destroyed) instance, so both operations are
    /// disabled. Use `std::unique_ptr<document>` when movable ownership is
    /// required.
    document(const document&) = delete;
    document& operator=(const document&) = delete;
    document(document&&) = delete;
    document& operator=(document&&) = delete;

    ~document() = default;

    /// Open an existing document at `path` in read mode.
    ///
    /// Returns a `document` configured with a file reader.
    ///
    /// @throws std::runtime_error if the file cannot be opened for reading.
    static document open(const std::filesystem::path& path);

    /// Create a document at `path` in write mode.
    ///
    /// Returns a `document` configured with a file writer.
    ///
    /// @throws std::runtime_error if the file cannot be opened for writing.
    static document create(const std::filesystem::path& path);

    /// Save the document to the configured writer backend.
    ///
    /// Uses the currently injected save strategy (see `set_save_strategy()`),
    /// which defaults to `consolidate_document_save_strategy` (full rewrite).
    ///
    /// @throws logic_exception if no writer or serializer is available.
    void save();

    /// Save the document using a specific built-in strategy identified by `type`.
    ///
    /// The injected strategy (if any) is replaced by the built-in implementation
    /// corresponding to `type`. After this call, a subsequent parameterless `save()`
    /// will use this built-in strategy, not the previously injected one.
    ///
    /// @throws logic_exception if no writer or serializer is available.
    void save(save_strategy_type type);

    /// Inject a custom save strategy.
    ///
    /// The document takes ownership of `strategy`.  All subsequent calls to the
    /// parameterless `save()` will delegate to this implementation.
    ///
    /// Pass `nullptr` to reset to the built-in default (`full_rewrite`).
    void set_save_strategy(std::unique_ptr<class document_save_strategy> strategy);

    /// Returns whether a reader backend is available.
    [[nodiscard]] bool has_reader() const;

    /// Returns whether a parser facade is available.
    [[nodiscard]] bool has_parser() const;

    /// Replace the resource limits used by subsequent parsing and decoding.
    void set_limits(const resource_limits& limits) noexcept;

    /// Returns the resource limits used by this document.
    [[nodiscard]] const resource_limits& limits() const noexcept;

    /// Returns whether a writer backend is available.
    [[nodiscard]] bool has_writer() const;

    /// Returns whether a serializer facade is available.
    [[nodiscard]] bool has_serializer() const;

    /// Access the underlying reader backend.
    ///
    /// Returns `nullptr` when no reader is configured.
    [[nodiscard]] ripper::io::core::reader* reader() const;

    /// Access the parser facade.
    ///
    /// Returns `nullptr` when no parser is configured.
    [[nodiscard]] class parser* parser() const;

    /// Access the underlying writer backend.
    ///
    /// Returns `nullptr` when no writer is configured.
    [[nodiscard]] ripper::io::core::writer* writer() const;

    /// Access the serializer facade.
    ///
    /// Returns `nullptr` when no serializer is configured.
    [[nodiscard]] class serializer* serializer() const;

    /// Return the PDF header object.
    ///
    /// If `reader` is set, the header is parsed from the input upon first access and cached
    /// for subsequent accesses. Otherwise, a new header with default values is created and cached.
    /// Changes made to the returned header are reflected in the document on save.
    [[nodiscard]] class header& header();

    /// Parse and return the cross-reference manager (cached).
    ///
    /// The cross-reference manager is a compiled view over all revisions with
    /// newest-wins lookup semantics for duplicate object numbers. Changes made
    /// to the returned manager are reflected in the document on save.
    [[nodiscard]] class cross_reference_manager& cross_reference_table();

    /// Parse and return the trailer manager (cached).
    ///
    /// Use `trailer().compiled()` for the merged view, or `trailer().active_trailer()`
    /// to modify the current revision's trailer.
    [[nodiscard]] class trailer_manager& trailer();

    /// Return the revision manager (cached).
    ///
    /// Provides direct access to the ordered list of revisions, the active revision,
    /// and the view-managers (xref, trailer). Use `revisions()` to enumerate all
    /// revisions for save strategies or introspection.
    [[nodiscard]] class revision_manager& revisions();

    /// Return a view over the document catalog.
    ///
    /// Resolves the catalog from the xref each time this method is called (not cached,
    /// unlike `header()`, `cross_reference_table()`, and `trailer()`). The returned
    /// `catalog` is a lightweight non-owning view. Ownership remains with the xref entry.
    [[nodiscard]] class catalog catalog();

    /// Resolve any indirect object by reference, lazy-loading from file if needed.
    ///
    /// Returns a raw non-owning pointer into the xref entry. The pointer remains valid
    /// as long as the document (and its xref) is alive.
    ///
    /// @throws logic_exception if the object is not found.
    class indirect_object* resolve_object(indirect_reference ref);

    /// Resolve an indirect object and ensure it exists in the active (newest) xref section.
    ///
    /// If the object is already in the active section, returns a reference to the existing
    /// object. If it exists in an older section, clones it to the active section and returns
    /// a reference to the clone.
    ///
    /// Use this when you intend to modify an object and want those modifications captured
    /// during incremental save.
    ///
    /// @throws logic_exception if the object is not found or no active section exists.
    class indirect_object& resolve_object_to_active_revision(indirect_reference ref);

    /// Create a new revision for incremental updates.
    ///
    /// Appends a new revision with a cross-reference section (pre-populated with object 0,
    /// the free-list head) and a matching trailer with `/Size` set and `/Prev` pointing to
    /// the previous revision's section `startxref_offset()` (if one exists).
    ///
    /// After calling this method, existing objects can be cloned into the new
    /// revision using `resolve_object_to_active_revision()` or by calling
    /// `rebind_to_active_revision()` on any instance of `object_view`. New
    /// objects can be added via `reserve()` / `allocate()` on the active section.
    ///
    /// Convenience wrappers (e.g. `pages::add_page()`) automatically rebind
    /// themselves to the active revision, so most users only need to call
    /// `create_new_revision()` before making changes and saving incrementally.
    ///
    /// @return A mutable reference to the newly created revision.
    class revision& create_new_revision();

private:
    /// Lazy-initialize and return the revision manager — the entrypoint through which
    /// all parsed document state (xref entries, trailers, revisions) is accessed.
    [[nodiscard]] class revision_manager& initialize_revision_manager();

    /// The underlying reader for the PDF file.
    std::unique_ptr<ripper::io::core::reader> reader_;
    /// The parser for the PDF file. Always present when a reader is available.
    std::unique_ptr<class parser> parser_;

    /// The underlying writer for the PDF file.
    std::unique_ptr<ripper::io::core::writer> writer_;
    /// The serializer for the PDF file. Always present when a writer is available.
    std::unique_ptr<class serializer> serializer_;

    /// The current save strategy implementation.
    std::unique_ptr<class document_save_strategy> save_strategy_;

    /// The parsed header of the PDF file.
    std::optional<class header> header_;

    /// The lazily-initialized revision manager (xrefs + trailers).
    std::unique_ptr<class revision_manager> revision_manager_;

    /// Limits applied to parsing and decoding operations.
    resource_limits limits_{};

    /// References currently being resolved, used to reject recursive resolution.
    std::unordered_set<indirect_reference> resolving_;
};
} // namespace ripper::pdf::core
