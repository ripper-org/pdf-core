#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"

#include "ripper/pdf/core/document/object/helpers/stream.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
namespace ripper::pdf::core
{
indirect_object::indirect_object(object_identity identity, class object content) noexcept
    : identity_(std::move(identity)), content_(std::move(content))
{
}

const object_identity& indirect_object::identity() const noexcept
{
    return identity_;
}

object_identity& indirect_object::identity() noexcept
{
    return identity_;
}

const object& indirect_object::content() const noexcept
{
    return content_;
}

object& indirect_object::content() noexcept
{
    return content_;
}

indirect_object indirect_object::clone() const
{
    return indirect_object{identity_, content_};
}
} // namespace ripper::pdf::core
