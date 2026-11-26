#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ripper::pdf::core
{
TEST_CASE("geometry rect normalizes corners", "[geometry]")
{
    const auto normalized =
        geometry::rect::from_corners(geometry::point{10.0, 20.0}, geometry::point{4.0, 5.0});

    REQUIRE(normalized == geometry::rect{4.0, 5.0, 6.0, 15.0});
}
} // namespace ripper::pdf::core
