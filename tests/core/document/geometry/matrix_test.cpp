#include "ripper/pdf/core/document/geometry/matrix.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace ripper::pdf::core::geometry
{
TEST_CASE("matrix composes in application order", "[geometry][matrix]")
{
    // Translate first, then scale: the translation is scaled along with the point.
    const auto transformed = matrix::translated(10.0, 0.0) * matrix::scaled(2.0, 2.0);

    REQUIRE(transformed == matrix{2.0, 0.0, 0.0, 2.0, 20.0, 0.0});
    REQUIRE(matrix::identity() * matrix::scaled(3.0, 4.0) == matrix::scaled(3.0, 4.0));
}

TEST_CASE("matrix rotation and skew match the PDF definitions", "[geometry][matrix]")
{
    const auto rotated = matrix::rotated(90.0);
    REQUIRE(rotated.a == Catch::Approx(0.0).margin(1e-12));
    REQUIRE(rotated.b == Catch::Approx(1.0));
    REQUIRE(rotated.c == Catch::Approx(-1.0));
    REQUIRE(rotated.d == Catch::Approx(0.0).margin(1e-12));

    REQUIRE(matrix::skewed(45.0, 0.0).c == Catch::Approx(1.0));
    REQUIRE(matrix::skewed(0.0, 45.0).b == Catch::Approx(1.0));
}
} // namespace ripper::pdf::core::geometry
