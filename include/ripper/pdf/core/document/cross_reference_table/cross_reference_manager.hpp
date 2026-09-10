#pragma once

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_table.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/revision.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace ripper::pdf::core
{
class indirect_object;
class cross_reference_entry;

/// Non-owning compiled view over all revisions in a revision_manager.
///
/// A PDF file may contain multiple cross-reference sections, one per incremental update.
/// The manager provides a unified entry point for all cross-reference operations,
/// scanning revisions from newest to oldest for lookups and compiling entry maps
/// from oldest to newest.
///
/// The manager does NOT own the revisions, they are owned by revision_manager.
/// The manager holds a reference to the revisions vector and must not outlive it.
///
/// ## Lookup semantics
///
/// When looking up an entry by object number, the manager scans revisions from
/// newest to oldest. The first match is returned, so the most recent revision of
/// any object always takes precedence over earlier revisions.
///
/// ## Allocation semantics
///
/// New indirect objects are allocated into the active revision's section (the
/// last, newest revision). If no revisions exist, the revision_manager must
/// provide at least one revision before allocation.
///
/// ## Compiled view
///
/// `entries()` returns a flat pointer map compiled across all revisions where
/// newest wins for duplicate object numbers. This is the canonical view of the
/// document's current state.
///
/// ## Ordering
///
/// Revisions are stored oldest-first (index 0 is the original revision,
/// `back()` is the most recently added revision). This mirrors the convention
/// used by `revision_manager`.
///
/// Implements the `cross_reference_table` interface, sharing the same API as
/// individual `cross_reference_section` objects for uniform access.
class cross_reference_manager : public cross_reference_table
{
public:
    /// Construct a manager as a non-owning view over a revisions vector.
    ///
    /// `revisions` must be in chronological order (oldest first, newest last).
    /// The manager stores a raw pointer and must not outlive the vector.
    explicit cross_reference_manager(std::vector<revision>& revisions) noexcept;

    /// Look up a mutable entry by object number across all revisions.
    ///
    /// Scans revisions from newest to oldest and returns the first matching entry,
    /// ensuring the most recent revision of an object always takes precedence.
    ///
    /// Returns a raw pointer to the entry (valid for the lifetime of this manager),
    /// or `nullptr` if no entry exists for the given object number in any revision.
    [[nodiscard]] cross_reference_entry* find(std::uint32_t object_number) noexcept override;

    /// Look up a read-only entry by object number across all revisions.
    ///
    /// Scans revisions from newest to oldest and returns the first matching entry,
    /// ensuring the most recent revision of an object always takes precedence.
    ///
    /// Returns a raw pointer to the entry (valid for the lifetime of this manager),
    /// or `nullptr` if no entry exists for the given object number in any revision.
    [[nodiscard]] const cross_reference_entry*
    find(std::uint32_t object_number) const noexcept override;

    /// Look up a mutable entry by exact indirect reference across all revisions.
    ///
    /// Scans revisions from newest to oldest and returns the first entry whose
    /// object number and generation both match. Unlike `find(object_number)`,
    /// this does NOT fall back to a newer revision with a different generation.
    ///
    /// Returns a raw pointer to the entry, or `nullptr` if not found.
    [[nodiscard]] cross_reference_entry* find(const indirect_reference& ref) noexcept override;

    /// Look up a read-only entry by exact indirect reference across all revisions.
    ///
    /// Scans revisions from newest to oldest and returns the first entry whose
    /// object number and generation both match. Unlike `find(object_number)`,
    /// this does NOT fall back to a newer revision with a different generation.
    ///
    /// Returns a raw pointer to the entry, or `nullptr` if not found.
    [[nodiscard]] const cross_reference_entry*
    find(const indirect_reference& ref) const noexcept override;

    /// Reserve a slot for a new indirect object and return its assigned indirect reference.
    ///
    /// Prefers recycling a free slot from the active revision's free list (PDF
    /// §7.5.4); when no free slot exists it computes the next available object
    /// number across all revisions. Creates a pending entry in the active
    /// revision's section, and returns the assigned indirect reference.
    /// The entry must be committed via `commit()` before it is usable.
    ///
    /// Use `allocate()` for the simpler single-step case.
    [[nodiscard]] indirect_reference reserve() override;

    /// Commit a constructed indirect object to a previously reserved reference.
    ///
    /// Locates the entry for `ref` across all revisions (scanning newest to oldest),
    /// transfers ownership of `object` into it, and marks it as resolved. Returns a
    /// raw non-owning pointer to the committed indirect object.
    ///
    /// Returns `nullptr` if `ref` is not found in any revision, or if the entry was not
    /// in a reserved (new and unresolved) state.
    [[nodiscard]] class indirect_object*
    commit(const indirect_reference& ref,
           std::unique_ptr<class indirect_object> object) noexcept override;

    /// Allocate and immediately commit a new in-memory indirect object.
    ///
    /// Combines `reserve()` and `commit()` into a single step. Prefers recycling
    /// a free slot from the active revision's free list (PDF §7.5.4); when no
    /// free slot exists it computes the next available object number across all
    /// revisions. Adds the resolved entry to the active revision's section, and
    /// returns the assigned indirect reference.
    [[nodiscard]] indirect_reference
    allocate(std::unique_ptr<class indirect_object> object) override;

    /// Mark the object referenced by `ref` as deleted (PDF 32000-1 §7.5.4).
    ///
    /// If the object is represented in the active revision's section, it is
    /// marked free there and threaded into that section's free list. If it only
    /// exists in an older on-disk revision, a fresh free entry is recorded in
    /// the active section so an incremental save persists the deletion. Object 0
    /// (the free-list head) is never modified.
    ///
    /// This is a no-op when `ref` cannot be found or is already free.
    void mark_deleted(const indirect_reference& ref) noexcept;

    /// Return a compiled flat non-owning pointer map of all entries across all revisions.
    ///
    /// Iterates revisions from oldest to newest, applying `insert_or_assign` so that newer
    /// entries overwrite older ones for the same object number. The result is the canonical
    /// compiled view of the document: for each object number, the newest revision wins.
    ///
    /// The returned map contains raw pointers into the underlying revision storage. Pointers
    /// are valid as long as this manager is not modified.
    [[nodiscard]] std::map<std::uint32_t, cross_reference_entry*> entries() override;

    /// Return a compiled flat non-owning pointer map of the active (live) entries.
    ///
    /// Builds the same newest-wins merged view as `entries()`, then filters out any entry
    /// where `in_use()` is false, with the single exception of object 0, which is always
    /// retained as the mandatory free-list head.
    ///
    /// The returned map contains raw pointers into the underlying revision storage. Pointers
    /// are valid as long as this manager is not modified.
    [[nodiscard]] std::map<std::uint32_t, cross_reference_entry*> active_entries();

    /// Returns the total number of entries across all revisions.
    ///
    /// Counts every entry in every revision, including entries for the same object number
    /// that appear in multiple revisions. To count unique objects, use `entries().size()`.
    [[nodiscard]] std::size_t size() const noexcept override;

    /// Returns the next available object number for allocation across all revisions.
    ///
    /// This is the maximum of `next_object_number()` over all revision sections, ensuring
    /// that newly allocated objects do not collide with any object from any revision.
    /// Returns 1 if no revisions or entries are present.
    [[nodiscard]] std::uint32_t next_object_number() const noexcept override;

    /// Returns a mutable reference to the active revision's section (last, newest).
    ///
    /// The revision_manager guarantees at least one revision exists after construction,
    /// so this never returns a dangling reference for a properly constructed history.
    [[nodiscard]] cross_reference_section& active_section();

private:
    std::vector<revision>* revisions_;
};
} // namespace ripper::pdf::core
