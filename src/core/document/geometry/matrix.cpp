#include "ripper/pdf/core/document/geometry/matrix.hpp"

#include <cmath>

namespace ripper::pdf::core::geometry
{
namespace
{
constexpr double degrees_to_radians = 3.14159265358979323846 / 180.0;
}

matrix matrix::rotated(double degrees)
{
    const double radians = degrees * degrees_to_radians;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);

    return matrix{cosine, sine, -sine, cosine, 0.0, 0.0};
}

matrix matrix::skewed(double x_degrees, double y_degrees)
{
    const double x_radians = x_degrees * degrees_to_radians;
    const double y_radians = y_degrees * degrees_to_radians;

    return matrix{1.0, std::tan(y_radians), std::tan(x_radians), 1.0, 0.0, 0.0};
}

} // namespace ripper::pdf::core::geometry
