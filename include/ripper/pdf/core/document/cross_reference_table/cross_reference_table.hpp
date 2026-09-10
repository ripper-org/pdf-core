#pragma once

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"

#include <cstdint>
#include <map>
#include <memory>

namespace ripper::pdf::core
{
class indirect_object;

/// Abstract interface shared by `cross_reference_section` and `cross_reference_manager`.
///
/// Provides a uniform API for looking up, allocating, and iterating cross-reference
/// entries, regardless of whether the caller operates on the full document table
/// (`cross_reference_manager`) or a single revision's section (`cross_reference_section`).
///
/// ## Object lifecycle
///
/// New indirect objects can be introduced via `allocate()` (single-step), or via the
/// `reserve()` / `commit()` pair when the object's constructor requires a known
/// `indirect_reference` before the entry can exist.
///
/// ## Address stability
///
/// Entry pointers returned by `find()` and `entries()` are stable for the lifetime of the
/// table as long as no mutation occurs. Entries live in `std::map` nodes; resolved objects
/// are owned exclusively by their entry via `std::unique_ptr`.
class cross_reference_table
{
public:
    /// Type alias for an owning map of object numbers to their cross-reference entries.
    ///
    /// Used as the storage type inside `cross_reference_subsection` and as a convenience
    /// alias when constructing or inspecting tables directly.
    using entry_map = std::map<std::uint32_t, cross_reference_entry>;

    virtual ~cross_reference_table() = default;

    /// Look up a mutable cross-reference entry by object number.
    ///
    /// Scans the table for an entry whose object number matches `object_number`.
    /// When multiple entries share the same object number (across revisions),
    /// the most recent entry wins.
    ///
    /// Returns a raw pointer into the table (valid for the lifetime of this table),
    /// or `nullptr` if no entry exists for the given object number.
    [[nodiscard]] virtual cross_reference_entry* find(std::uint32_t object_number) noexcept = 0;

    /// Look up a read-only cross-reference entry by object number.
    ///
    /// Scans the table for an entry whose object number matches `object_number`.
    /// When multiple entries share the same object number (across revisions),
    /// the most recent entry wins.
    ///
    /// Returns a raw pointer into the table (valid for the lifetime of this table),
    /// or `nullptr` if no entry exists for the given object number.
    [[nodiscard]] virtual const cross_reference_entry*
    find(std::uint32_t object_number) const noexcept = 0;

    /// Look up a mutable cross-reference entry by exact indirect reference.
    ///
    /// Both `ref.object_number()` and `ref.generation()` must match.
    /// This is the primary lookup. Prefer it over `find(object_number)` when
    /// you hold a specific indirect reference.
    ///
    /// Returns a raw pointer into the table, or `nullptr` if not found.
    [[nodiscard]] virtual cross_reference_entry* find(const indirect_reference& ref) noexcept = 0;

    /// Look up a read-only cross-reference entry by exact indirect reference.
    ///
    /// Both `ref.object_number()` and `ref.generation()` must match.
    /// This is the primary lookup, prefer it over `find(object_number)` when
    /// you hold a specific indirect reference.
    ///
    /// Returns a raw pointer into the table, or `nullptr` if not found.
    [[nodiscard]] virtual const cross_reference_entry*
    find(const indirect_reference& ref) const noexcept = 0;

    /// Look up the most recent mutable entry whose object number matches `ref.object_number()`.
    ///
    /// The generation number in `ref` is ignored. Only the object number is used.
    /// When multiple entries share the same object number, the newest revision wins.
    [[nodiscard]] cross_reference_entry* find_most_recent(const indirect_reference& ref) noexcept
    {
        return find(ref.object_number());
    }

    /// Look up the most recent read-only entry whose object number matches `ref.object_number()`.
    ///
    /// The generation number in `ref` is ignored, only the object number is used.
    /// When multiple entries share the same object number, the newest revision wins.
    [[nodiscard]] const cross_reference_entry*
    find_most_recent(const indirect_reference& ref) const noexcept
    {
        return find(ref.object_number());
    }

    /// Look up the most recent mutable entry by object number.
    ///
    /// Equivalent to `find(object_number)` with an explicit name.
    [[nodiscard]] cross_reference_entry* find_most_recent(std::uint32_t object_number) noexcept
    {
        return find(object_number);
    }

    /// Look up the most recent read-only entry by object number.
    ///
    /// Equivalent to `find(object_number)` with an explicit name.
    [[nodiscard]] const cross_reference_entry*
    find_most_recent(std::uint32_t object_number) const noexcept
    {
        return find(object_number);
    }

    /// Reserve a slot for a new indirect object and return its assigned indirect reference.
    ///
    /// Creates a pending entry with no indirect object and no file offset. The returned
    /// `indirect_reference` can be passed to the indirect object's constructor, then the
    /// constructed object must be submitted via `commit()` before the entry is usable.
    ///
    /// Use this two-step pair when the indirect object requires its own reference during
    /// construction. Use `allocate()` for the simpler single-step case.
    [[nodiscard]] virtual indirect_reference reserve() = 0;

    /// Commit a constructed indirect object to a previously reserved reference.
    ///
    /// Transfers ownership of `object` into the entry identified by `ref` and marks the
    /// entry as resolved. Returns a raw non-owning pointer to the committed indirect object.
    ///
    /// Returns `nullptr` if:
    ///   - `ref` does not correspond to any entry in this table, or
    ///   - the entry was not previously reserved (i.e. it is not new and unresolved).
    [[nodiscard]] virtual class indirect_object*
    commit(const indirect_reference& ref,
           std::unique_ptr<class indirect_object> object) noexcept = 0;

    /// Allocate and immediately commit a new in-memory indirect object.
    ///
    /// Combines `reserve()` and `commit()` into a single step. The next available object
    /// number is assigned, the entry is created and resolved in one operation, and the
    /// assigned indirect reference is returned.
    ///
    /// Use this when the indirect object does not need to know its own reference during
    /// construction. Use `reserve()` / `commit()` for the two-step case.
    [[nodiscard]] virtual indirect_reference
    allocate(std::unique_ptr<class indirect_object> object) = 0;

    /// Return a flat non-owning pointer map of all entries.
    ///
    /// For `cross_reference_section`, spans all entries across its subsections in a
    /// single map keyed by object number.
    /// For `cross_reference_manager`, returns the compiled view across all sections,
    /// where the newest entry wins for any object number that appears in multiple revisions.
    ///
    /// The returned map contains raw pointers into the underlying storage. Pointers are
    /// valid as long as this table is not modified.
    [[nodiscard]] virtual std::map<std::uint32_t, cross_reference_entry*> entries() = 0;

    /// Returns the total number of entries across this table.
    ///
    /// For `cross_reference_section`, this is the sum of all subsection entry counts.
    /// For `cross_reference_manager`, this is the sum across all sections, including
    /// duplicate object numbers from different revisions.
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

    /// Returns the next available object number for allocation.
    ///
    /// This is one greater than the highest object number currently present in this table.
    /// Returns 1 if the table contains no entries (object number 0 is reserved for the
    /// free-list head and is not a valid allocation target).
    ///
    /// Used internally by `reserve()` and `allocate()`, but exposed for introspection.
    [[nodiscard]] virtual std::uint32_t next_object_number() const noexcept = 0;
};
} // namespace ripper::pdf::core
