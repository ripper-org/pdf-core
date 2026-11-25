#pragma once

#include "ripper/pdf/core/document/geometry/rect.hpp"

namespace ripper::pdf::core
{
/// Common page sizes in PDF points (1 pt = 1/72 inch, ISO 32000-1 §14.11.2).
enum class page_size
{
    a3,
    a4,
    a5,
    a6,
    letter,
    legal,
    tabloid
};

/// Orientation applied to a `page_size` when deriving a /MediaBox.
enum class page_orientation
{
    portrait,
    landscape
};

/// Returns the /MediaBox for `size` in the requested orientation.
///
/// The rectangle always starts at the origin. Landscape swaps width and height.
[[nodiscard]] constexpr geometry::rect
media_box(page_size size, page_orientation orientation = page_orientation::portrait) noexcept
{
    double width = 0.0;
    double height = 0.0;

    switch (size)
    {
        case page_size::a3:
            width = 842.0;
            height = 1191.0;
            break;
        case page_size::a4:
            width = 595.0;
            height = 842.0;
            break;
        case page_size::a5:
            width = 420.0;
            height = 595.0;
            break;
        case page_size::a6:
            width = 298.0;
            height = 420.0;
            break;
        case page_size::letter:
            width = 612.0;
            height = 792.0;
            break;
        case page_size::legal:
            width = 612.0;
            height = 1008.0;
            break;
        case page_size::tabloid:
            width = 792.0;
            height = 1224.0;
            break;
    }

    if (orientation == page_orientation::landscape)
        return geometry::rect{0.0, 0.0, height, width};

    return geometry::rect{0.0, 0.0, width, height};
}
} // namespace ripper::pdf::core
