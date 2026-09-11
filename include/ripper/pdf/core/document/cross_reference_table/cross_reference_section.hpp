#pragma once

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_table.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>

namespace ripper::pdf::core
{
class indirect_object;
class cross_reference_entry;

/// A single cross-reference section in a PDF file.
///
/// Per the PDF spec (§7.5.4), a cross-reference section begins with the `xref` keyword
/// and contains one or more subsections. One new section is added each time the file is
/// incrementally updated. A brand-new document contains exactly one section.
///
/// Entries within a section are grouped into `cross_reference_subsection` objects, each
/// covering a contiguous run of object numbers. New entries are appended via `add_entry()`,
/// which automatically merges consecutive object numbers into the last subsection or starts
/// a new one when there is a gap.
///
/// Implements the `cross_reference_table` interface, so it can be used interchangeably
/// with `cross_reference_manager` for lookup and modification.
class cross_reference_section : public cross_reference_table
{
public:
    /// Construct a section from a set of pre-built subsections.
    ///
    /// `subsections` contains the initial subsections for this section, which may be empty
    /// for a brand-new section to which entries will be added via `add_entry()`.
    ///
    /// `startxref_offset`, when provided, is the byte offset in the file at which the
    /// `xref` keyword for this section appears. Can be set or updated later via
    /// `set_startxref_offset()`.
    explicit cross_reference_section(
        std::deque<cross_reference_subsection> subsections,
        std::optional<std::uint64_t> startxref_offset = std::nullopt) noexcept;

    cross_reference_section(const cross_reference_section&) = default;
    cross_reference_section& operator=(const cross_reference_section&) = default;
    cross_reference_section(cross_reference_section&&) noexcept = default;
    cross_reference_section& operator=(cross_reference_section&&) noexcept = default;

    /// Look up a mutable entry by object number within this section.
    ///
    /// Scans all subsections and returns a raw pointer to the matching entry
    /// (valid for the lifetime of this section), or `nullptr` if not found.
    [[nodiscard]] cross_reference_entry* find(std::uint32_t object_number) noexcept override;

    /// Look up a read-only entry by object number within this section.
    ///
    /// Scans all subsections and returns a raw pointer to the matching entry
    /// (valid for the lifetime of this section), or `nullptr` if not found.
    [[nodiscard]] const cross_reference_entry*
    find(std::uint32_t object_number) const noexcept override;

    /// Look up a mutable entry by exact indirect reference within this section.
    ///
    /// Both `ref.object_number()` and `ref.generation()` must match.
    /// Returns a raw pointer to the entry, or `nullptr` if not found.
    [[nodiscard]] cross_reference_entry* find(const indirect_reference& ref) noexcept override;

    /// Look up a read-only entry by exact indirect reference within this section.
    ///
    /// Both `ref.object_number()` and `ref.generation()` must match.
    /// Returns a raw pointer to the entry, or `nullptr` if not found.
    [[nodiscard]] const cross_reference_entry*
    find(const indirect_reference& ref) const noexcept override;

    /// Reserve a slot for a new indirect object within this section.
    ///
    /// Prefers recycling a free slot from this section's free list (PDF
    /// §7.5.4); only when no free slot exists does it assign one greater than
    /// the highest object number present. Creates a pending entry via
    /// `add_entry()` and returns the assigned indirect reference.
    /// The entry must be committed via `commit()` before it is usable.
    ///
    /// Use `allocate()` for the simpler single-step case.
    [[nodiscard]] indirect_reference reserve() override;

    /// Commit a constructed indirect object to a previously reserved entry in this section.
    ///
    /// Looks up the entry for `ref` within this section, transfers ownership of `object`
    /// into it, and marks it as resolved. Returns a raw non-owning pointer to the committed
    /// indirect object.
    ///
    /// Returns `nullptr` if `ref` is not found in this section, or if the entry was not
    /// in a reserved (new and unresolved) state.
    [[nodiscard]] class indirect_object*
    commit(const indirect_reference& ref,
           std::unique_ptr<class indirect_object> object) noexcept override;

    /// Allocate and immediately commit a new in-memory indirect object within this section.
    ///
    /// Combines `reserve()` and `commit()` into a single step. Prefers recycling
    /// a free slot from this section's free list (PDF §7.5.4); only when no free
    /// slot exists does it assign one greater than the highest object number
    /// present. Creates and resolves the entry in one operation, and returns the
    /// assigned indirect reference.
    [[nodiscard]] indirect_reference
    allocate(std::unique_ptr<class indirect_object> object) override;

    /// Return a flat non-owning pointer map of all entries across all subsections.
    ///
    /// The returned map is keyed by object number and contains raw pointers into the
    /// subsection storage. Pointers are valid as long as this section is not modified.
    [[nodiscard]] std::map<std::uint32_t, cross_reference_entry*> entries() override;

    /// Returns the total number of entries across all subsections in this section.
    [[nodiscard]] std::size_t size() const noexcept override;

    /// Returns the next available object number for allocation within this section.
    ///
    /// This is one greater than the highest object number present across all subsections.
    /// Returns 1 if this section contains no entries.
    [[nodiscard]] std::uint32_t next_object_number() const noexcept override;

    /// Returns a read-only view of the subsections in this section.
    ///
    /// Subsections are stored in the order they were added, each covering a contiguous
    /// range of object numbers. The returned reference is valid for the lifetime of
    /// this section.
    [[nodiscard]] const std::deque<cross_reference_subsection>& subsections() const noexcept;

    /// Returns a mutable view of the subsections in this section.
    ///
    /// The returned reference is valid for the lifetime of this section.
    [[nodiscard]] std::deque<cross_reference_subsection>& subsections() noexcept;

    /// Returns the byte offset in the file at which the `xref` keyword for this section resides.
    ///
    /// Returns `std::nullopt` for sections that have not yet been written to disk,
    /// or for in-memory sections created during document construction.
    [[nodiscard]] std::optional<std::uint64_t> startxref_offset() const noexcept;

    /// Set the byte offset at which the `xref` keyword for this section resides in the file.
    ///
    /// Called by the parser after locating this section's position in the file, or by the
    /// serializer after writing the section to record its final offset.
    void set_startxref_offset(std::uint64_t offset) noexcept;

    /// Returns `true` if this section is a compressed cross-reference stream (PDF 1.5+).
    ///
    /// A compressed xref section serializes as an indirect object (the xref stream) rather
    /// than as a traditional `xref` keyword + `trailer` block. The object number of the xref
    /// stream is available via `xref_stream_object_number()`.
    [[nodiscard]] bool is_compressed() const noexcept;

    /// Returns the object number of the cross-reference stream indirect object for this section.
    ///
    /// Only meaningful when `is_compressed()` is `true`; returns `std::nullopt` for
    /// traditional cross-reference table sections.
    [[nodiscard]] std::optional<std::uint32_t> xref_stream_object_number() const noexcept;

    /// Set the object number of the cross-reference stream indirect object for this section.
    ///
    /// Called by the parser when it encounters a compressed xref stream, or by the save
    /// strategy when it reserves an object number for the xref stream. Setting this to a
    /// value makes `is_compressed()` return `true`; clearing it (passing `std::nullopt`)
    /// makes the section a traditional xref table section again.
    void set_xref_stream_object_number(std::optional<std::uint32_t> object_number) noexcept;

    /// Append a new entry to this section, grouping it into a subsection automatically.
    ///
    /// If the entry's object number is exactly one greater than the last object number in
    /// the current (last) subsection, the entry is appended to that subsection, extending
    /// its contiguous run. If there is a gap, or if this section has no subsections yet,
    /// a new subsection is created starting at the entry's object number.
    void add_entry(cross_reference_entry entry);

    /// Take a free slot from this section's free list (PDF §7.5.4).
    ///
    /// Follows the free list starting at the head (object 0), removes the first
    /// free entry from this section, and relinks the list. Returns the recycled
    /// reference (object number + reuse generation) so the caller can create the
    /// replacement entry, or `std::nullopt` when no free slot is available.
    ///
    /// The head is object 0; if it is missing or the free list is malformed this
    /// is a no-op returning `std::nullopt`.
    [[nodiscard]] std::optional<indirect_reference> take_free_slot() noexcept;

    /// Link the entry at `object_number` into this section's free list (§7.5.4).
    ///
    /// The entry must already be marked free (see `cross_reference_entry::mark_deleted`).
    /// A no-op if object 0 or the entry is missing.
    void link_free(std::uint32_t object_number) noexcept;

    /// Mark `ref` as deleted in this section, threading the free list (§7.5.4).
    ///
    /// A no-op if the entry does not exist in this section or is the object-0 head.
    void mark_deleted(const indirect_reference& ref) noexcept;

    /// Create a new entry in this section as a deep copy of `source`.
    ///
    /// The indirect object (if resolved) is fully cloned so the new entry owns an
    /// independent copy.  The returned pointer points into this section's storage
    /// and is valid until the section is modified.
    ///
    /// @return A non-owning pointer to the newly added entry, or `nullptr` if
    ///         the entry could not be found after insertion.
    [[nodiscard]] cross_reference_entry* add_entry_from(const cross_reference_entry& source);

private:
    std::deque<cross_reference_subsection> subsections_;
    std::optional<std::uint64_t> startxref_offset_;
    std::optional<std::uint32_t> xref_stream_object_number_;
};
} // namespace ripper::pdf::core
