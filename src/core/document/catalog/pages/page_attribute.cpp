#include "page_attribute.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ripper::pdf::core::page_attribute
{
namespace
{
/// Reads one box coordinate, dereferencing indirect numbers. Returns `nullopt`
/// when the entry is not a number.
[[nodiscard]] std::optional<double> box_number(document& doc, const array_object& box,
                                               std::size_t index)
{
    const auto* item = &box.at(index);

    if (const auto* ref = item->as_indirect_reference())
    {
        auto* resolved = doc.resolve_object(*ref);
        if (resolved == nullptr)
            return std::nullopt;

        item = &resolved->content();
    }

    const auto* number = item->as_number();
    if (number == nullptr)
        return std::nullopt;

    return number->as_real();
}
} // namespace

int normalize_rotation(std::int64_t degrees) noexcept
{
    return static_cast<int>(((degrees % 360) + 360) % 360);
}

std::optional<geometry::rect> read_box(document& doc, const object& value)
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

    return geometry::rect::from_corners({*lower_left_x, *lower_left_y},
                                        {*upper_right_x, *upper_right_y});
}

const object* local_entry(indirect_object& node, std::string_view key)
{
    const auto* dict = node.content().as_dictionary();
    if (dict == nullptr)
        return nullptr;

    const auto* value = dict->get(std::string{key});
    if (value == nullptr)
        return nullptr;

    if (const auto* ref = value->as_indirect_reference())
    {
        auto* resolved = node.identity().owner().resolve_object(*ref);
        if (resolved == nullptr || resolved->content().is_null())
            return nullptr;

        return &resolved->content();
    }

    if (value->is_null())
        return nullptr;

    return value;
}
} // namespace ripper::pdf::core::page_attribute
