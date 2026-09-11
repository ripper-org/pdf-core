#pragma once

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/trailer/trailer_manager.hpp"

#include <deque>

namespace ripper::pdf::core
{
/// The complete revision history of a PDF document.
///
/// Owns all revisions in chronological order (oldest first, newest last) and
/// composes the cross-reference manager and trailer manager as non-owning
/// compiled views bound to the revisions vector. The views are held as
/// raw pointers for copyability and mobility.
class revision_manager
{
public:
    /// Construct a revision manager from a chronologically ordered list of revisions.
    ///
    /// `revisions` must be in oldest-first order. Typically the parser builds this
    /// list by collecting revisions newest-first (following /Prev) and then
    /// reversing before constructing the manager.
    explicit revision_manager(std::deque<revision> revisions);

    /// Returns a mutable reference to the ordered list of all revisions (oldest first).
    [[nodiscard]] std::deque<revision>& all() noexcept;

    /// Returns a const reference to the ordered list of all revisions (oldest first).
    [[nodiscard]] const std::deque<revision>& all() const noexcept;

    /// Returns a mutable reference to the current (newest) revision.
    [[nodiscard]] revision& current();

    /// Returns a const reference to the current (newest) revision.
    [[nodiscard]] const revision& current() const noexcept;

    /// Append a new revision as the most recent update.
    void push(class revision r);

    /// Returns a mutable reference to the cross-reference manager (compiled view).
    [[nodiscard]] cross_reference_manager& xref() noexcept;

    /// Returns a const reference to the cross-reference manager.
    [[nodiscard]] const cross_reference_manager& xref() const noexcept;

    /// Returns a mutable reference to the trailer manager (compiled view).
    [[nodiscard]] trailer_manager& trailer() noexcept;

    /// Returns a const reference to the trailer manager.
    [[nodiscard]] const trailer_manager& trailer() const noexcept;

    /// Consolidate all revisions into a single revision.
    ///
    /// Computes the newest-wins active entry view across all sections, moves those
    /// entries into a fresh section, compiles the trailer merging oldest-to-newest
    /// and stripping /Prev, and replaces the revision list with a single revision.
    /// Used when performing a full save/rewrite.
    void squash();

    /// Returns the number of revisions in this history.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    // The list of revisions in chronological order (oldest first, newest last).
    std::deque<revision> revisions_;

    // Compiled cross-reference manager view over all revisions.
    cross_reference_manager xref_view_;

    // Compiled trailer manager view over all revisions.
    class trailer_manager trailer_view_;
};
} // namespace ripper::pdf::core
