#include "ripper/pdf/core/document/graphics/color.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace ripper::pdf::core::graphics
{
TEST_CASE("color exposes its model and components", "[color]")
{
    const auto gray = color::gray(0.5);
    REQUIRE(gray.get_model() == color::model::gray);
    REQUIRE(gray.components().size() == 1);
    REQUIRE(gray.components()[0] == 0.5);

    const auto rgb = color::rgb(0.1, 0.2, 0.3);
    REQUIRE(rgb.get_model() == color::model::rgb);
    REQUIRE(rgb.components().size() == 3);
    REQUIRE(rgb.components()[0] == 0.1);
    REQUIRE(rgb.components()[1] == 0.2);
    REQUIRE(rgb.components()[2] == 0.3);

    const auto cmyk = color::cmyk(0.1, 0.2, 0.3, 0.4);
    REQUIRE(cmyk.get_model() == color::model::cmyk);
    REQUIRE(cmyk.components().size() == 4);
    REQUIRE(cmyk.components()[3] == 0.4);
}

TEST_CASE("color validates device components", "[color]")
{
    REQUIRE_THROWS_AS(color::gray(-0.1), logic_exception);
    REQUIRE_THROWS_AS(color::gray(1.1), logic_exception);
    REQUIRE_THROWS_AS(color::rgb(0.0, -0.1, 0.0), logic_exception);
    REQUIRE_THROWS_AS(color::cmyk(0.0, 0.0, 0.0, 2.0), logic_exception);
    REQUIRE_THROWS_AS(color::gray(std::numeric_limits<double>::quiet_NaN()), logic_exception);
    REQUIRE_THROWS_AS(color::rgb(0.0, 0.0, std::numeric_limits<double>::infinity()),
                      logic_exception);
}
} // namespace ripper::pdf::core::graphics
