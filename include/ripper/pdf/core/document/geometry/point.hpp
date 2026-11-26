#pragma once

namespace ripper::pdf::core::geometry
{

/// A point in PDF user space or in the current path's coordinate system.
///
/// Coordinates are unconstrained here; the brush converts them to PDF reals on
/// emission, which rejects non-finite values (`parse_exception`).
struct point
{
    double x = 0.0;
    double y = 0.0;

    bool operator==(const point& other) const noexcept = default;
};

} // namespace ripper::pdf::core::geometry
