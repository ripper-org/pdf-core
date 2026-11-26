#include "ripper/pdf/core/document/object/helpers/direct_object_view.hpp"

#include "ripper/pdf/core/document.hpp"

namespace ripper::pdf::core
{
direct_object_view::direct_object_view(object& value, indirect_object& owner) noexcept
    : value_{&value}, owner_{&owner}
{
}

object& direct_object_view::value() noexcept
{
    return *value_;
}

const object& direct_object_view::value() const noexcept
{
    return *value_;
}

indirect_object& direct_object_view::owner() noexcept
{
    return *owner_;
}

const indirect_object& direct_object_view::owner() const noexcept
{
    return *owner_;
}

document& direct_object_view::document() const
{
    return owner_->identity().owner();
}

void direct_object_view::rebind(object& value, indirect_object& owner) noexcept
{
    value_ = &value;
    owner_ = &owner;
}
} // namespace ripper::pdf::core
