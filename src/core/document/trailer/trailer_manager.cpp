#include "ripper/pdf/core/document/trailer/trailer_manager.hpp"

#include "ripper/pdf/core/document/object/object.hpp"

#include <utility>

namespace ripper::pdf::core
{
trailer_manager::trailer_manager(std::deque<revision>& revisions) noexcept : revisions_{&revisions}
{
}

trailer& trailer_manager::active_trailer()
{
    return revisions_->back().trailer();
}

const trailer& trailer_manager::active_trailer() const noexcept
{
    return revisions_->back().trailer();
}

trailer trailer_manager::compiled() const
{
    dictionary_object merged{};

    for (const auto& rev : *revisions_)
    {
        for (const auto& [key, val] : rev.trailer().dictionary().entries())
            merged.set(key, val);
    }

    return trailer{std::move(merged)};
}

std::size_t trailer_manager::size() const noexcept
{
    return revisions_->size();
}
} // namespace ripper::pdf::core
