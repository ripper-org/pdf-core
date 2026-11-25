#include "ripper/pdf/core/document/catalog/pages/page/rotation.hpp"

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstdint>

namespace ripper::pdf::core
{

page_rotation::page_rotation(std::int64_t degrees)
{
    const auto normalized = static_cast<int>(((degrees % 360) + 360) % 360);
    if (normalized % 90 != 0)
        throw logic_exception{"Page /Rotate must be a multiple of 90 degrees"};

    degrees_ = normalized;
}

page_rotation page_rotation::from(const object& value)
{
    const auto* number = value.as_number();
    if (number == nullptr)
        throw logic_exception{"Page /Rotate is not a number"};

    return page_rotation{number->as_integer()};
}

int page_rotation::degrees() const noexcept
{
    return degrees_;
}

bool page_rotation::is_quarter_turn() const noexcept
{
    return degrees_ == 90 || degrees_ == 270;
}
} // namespace ripper::pdf::core
