#include "ripper/pdf/core/document/revision_manager.hpp"

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"

#include <utility>

namespace ripper::pdf::core
{
revision_manager::revision_manager(std::deque<revision> revisions)
    : revisions_{std::move(revisions)}, xref_view_{revisions_}, trailer_view_{revisions_}
{
}

std::deque<revision>& revision_manager::all() noexcept
{
    return revisions_;
}

const std::deque<revision>& revision_manager::all() const noexcept
{
    return revisions_;
}

revision& revision_manager::current()
{
    return revisions_.back();
}

const revision& revision_manager::current() const noexcept
{
    return revisions_.back();
}

void revision_manager::push(class revision r)
{
    revisions_.push_back(std::move(r));
}

cross_reference_manager& revision_manager::xref() noexcept
{
    return xref_view_;
}

const cross_reference_manager& revision_manager::xref() const noexcept
{
    return xref_view_;
}

trailer_manager& revision_manager::trailer() noexcept
{
    return trailer_view_;
}

const trailer_manager& revision_manager::trailer() const noexcept
{
    return trailer_view_;
}

void revision_manager::squash()
{
    auto active = xref_view_.active_entries();

    cross_reference_section consolidated{{}};

    for (auto& [num, ptr] : active)
        consolidated.add_entry(std::move(*ptr));

    auto compiled_trailer = trailer_view_.compiled();
    compiled_trailer.dictionary().remove("Prev");

    revisions_.clear();
    revisions_.emplace_back(std::move(consolidated), std::move(compiled_trailer));
}

std::size_t revision_manager::size() const noexcept
{
    return revisions_.size();
}
} // namespace ripper::pdf::core
