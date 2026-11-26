#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"

#include "ripper/pdf/core/document.hpp"

namespace ripper::pdf::core
{
indirect_object_view::indirect_object_view(indirect_object& obj) noexcept : obj_{obj} {}

indirect_object& indirect_object_view::obj() noexcept
{
    return obj_.get();
}

const indirect_object& indirect_object_view::obj() const noexcept
{
    return obj_.get();
}

void indirect_object_view::rebind(indirect_object& obj) noexcept
{
    obj_ = std::ref(obj);
}

void indirect_object_view::rebind_to_active_revision()
{
    auto& active_obj = obj_.get().identity().owner().resolve_object_to_active_revision(
        obj_.get().identity().reference());

    obj_ = std::ref(active_obj);
}
} // namespace ripper::pdf::core
