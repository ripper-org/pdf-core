#include "ripper/pdf/core/document/object/number_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>

namespace ripper::pdf::core
{
TEST_CASE("number_object truncates representable reals", "[document][object][number]")
{
    REQUIRE(number_object{1.9}.as_integer() == 1);
    REQUIRE(number_object{-1.9}.as_integer() == -1);
    REQUIRE(number_object{0.0}.as_integer() == 0);
}

TEST_CASE("number_object rejects reals outside the int64 range", "[document][object][number]")
{
    REQUIRE_THROWS_AS((void)number_object{1e30}.as_integer(), parse_exception);
    REQUIRE_THROWS_AS((void)number_object{-1e30}.as_integer(), parse_exception);

    // 2^63 is exactly representable as a double but not as int64_t.
    const double two_to_63 = 9223372036854775808.0;
    REQUIRE_THROWS_AS((void)number_object{two_to_63}.as_integer(), parse_exception);
}

TEST_CASE("number_object accepts the minimum int64 real boundary", "[document][object][number]")
{
    const double min_int64 = -9223372036854775808.0;
    REQUIRE(number_object{min_int64}.as_integer() == std::numeric_limits<std::int64_t>::min());
}

TEST_CASE("number_object still rejects non-finite reals at construction",
          "[document][object][number]")
{
    REQUIRE_THROWS_AS((void)number_object{std::numeric_limits<double>::infinity()},
                      parse_exception);
    REQUIRE_THROWS_AS((void)number_object{std::numeric_limits<double>::quiet_NaN()},
                      parse_exception);
}
} // namespace ripper::pdf::core
