#pragma once

#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"

#include <cstddef>
#include <deque>

namespace ripper::pdf::core
{
/// Non-owning compiled view over the trailer history of a revision_manager.
///
/// A PDF document with incremental updates contains one trailer per revision.
/// The trailer_manager provides a compiled (merged) view and active-trailer
/// access over the revisions owned by revision_manager.
///
/// The manager does NOT own the revisions, they are owned by revision_manager.
/// The manager holds a reference to the revisions vector and must not outlive it.
///
/// ## Ordering
///
/// Revisions are stored oldest-first (index 0 is the original revision's trailer,
/// `back()` is the most recently added revision). This mirrors the ordering
/// convention used by `revision_manager`.
///
/// ## Compiled view
///
/// `compiled()` iterates oldest-to-newest and merges all dictionaries so that
/// newer values overwrite older ones. This matches the PDF specification rule
/// that the trailer chain must be followed from the most recent trailer back
/// to the first, with later values taking precedence.
///
/// ## Active trailer
///
/// `active_trailer()` returns a reference to the most recently added revision's
/// trailer. Writers that produce a new revision modify the active trailer.
class trailer_manager
{
public:
    /// Construct a trailer manager as a non-owning view over a revisions vector.
    ///
    /// `revisions` must be in chronological order (oldest first, newest last).
    /// The manager stores a raw pointer and must not outlive the vector.
    explicit trailer_manager(std::deque<revision>& revisions) noexcept;

    /// Returns a mutable reference to the active (newest) trailer.
    ///
    /// The active trailer is the one modified by writers when producing a new
    /// revision. It is always the last element of the internal vector. The
    /// revision_manager guarantees at least one revision exists after construction.
    [[nodiscard]] trailer& active_trailer();

    /// Returns a const reference to the active (newest) trailer.
    ///
    /// Requires that at least one revision has been added. The revision_manager
    /// guarantees this after construction.
    [[nodiscard]] const trailer& active_trailer() const noexcept;

    /// Returns the compiled (merged) trailer dictionary.
    ///
    /// Iterates all stored revisions oldest-to-newest and merges their dictionary
    /// entries so that newer values overwrite older ones. The result is a new
    /// `trailer` object computed on each call; it is not cached.
    ///
    /// Returns an empty-dictionary_object trailer if no revisions have been added.
    [[nodiscard]] trailer compiled() const;

    /// Returns the number of revisions (each carrying one trailer).
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::deque<revision>* revisions_;
};
} // namespace ripper::pdf::core
