#pragma once

#include "ripper/pdf/core/document/geometry/point.hpp"

#include <algorithm>

namespace ripper::pdf::core::geometry
{

/// An axis-aligned rectangle in PDF user space.
///
/// `width` and `height` are extents rather than a second corner, matching the
/// operands of the `re` operator (ISO 32000-1 §8.5.2.1). Use `from_corners()` to
/// build a rectangle from two arbitrary corners.
struct rect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    /// Returns the rectangle spanning `first` and `second`.
    ///
    /// Corners may be supplied in any order; negative extents are normalized so
    /// `width` and `height` are never negative.
    [[nodiscard]] static rect from_corners(point first, point second) noexcept
    {
        const double left = std::min(first.x, second.x);
        const double bottom = std::min(first.y, second.y);

        return rect{
            left,
            bottom,
            std::max(first.x, second.x) - left,
            std::max(first.y, second.y) - bottom,
        };
    }

    bool operator==(const rect& other) const noexcept = default;
};

} // namespace ripper::pdf::core::geometry
