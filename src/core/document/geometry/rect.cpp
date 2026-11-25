#include "ripper/pdf/core/document/geometry/rect.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <optional>

namespace ripper::pdf::core::geometry
{
namespace
{
/// Reads one box coordinate, dereferencing indirect numbers. Returns `nullopt`
/// when the entry is not a number.
[[nodiscard]] std::optional<double> box_number(document& doc, const array_object& box,
                                               std::size_t index)
{
    const auto* item = doc.resolve_object_deep(&box.at(index));

    const auto* number = item != nullptr ? item->as_number() : nullptr;
    if (number == nullptr)
        return std::nullopt;

    return number->as_real();
}
} // namespace

std::optional<rect> rect::from(document& doc, const object& value)
{
    const auto* box = value.as_array();
    if (box == nullptr || box->size() != 4)
        return std::nullopt;

    const auto lower_left_x = box_number(doc, *box, 0);
    const auto lower_left_y = box_number(doc, *box, 1);
    const auto upper_right_x = box_number(doc, *box, 2);
    const auto upper_right_y = box_number(doc, *box, 3);

    if (!lower_left_x || !lower_left_y || !upper_right_x || !upper_right_y)
        return std::nullopt;

    return from_corners({*lower_left_x, *lower_left_y}, {*upper_right_x, *upper_right_y});
}
} // namespace ripper::pdf::core::geometry
