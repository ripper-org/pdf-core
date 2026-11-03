#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/xref_field_width.hpp"
#include "ripper/pdf/core/util/byte_count.hpp"
#include "ripper/pdf/core/util/byte_offset.hpp"
#include "ripper/pdf/core/util/byte_range.hpp"
#include "ripper/pdf/core/util/checked_numeric.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace ripper::pdf::core
{
namespace
{
constexpr auto max_size = std::numeric_limits<std::size_t>::max();
}

static_assert(checked_numeric::integer<int>);
static_assert(checked_numeric::integer<std::size_t>);
static_assert(checked_numeric::unsigned_integer<std::uint16_t>);
static_assert(!checked_numeric::integer<bool>);
static_assert(!checked_numeric::integer<char>);
static_assert(!checked_numeric::integer<wchar_t>);
static_assert(!checked_numeric::unsigned_integer<int>);

static_assert(checked_numeric::narrow<std::uint16_t>(42) == 42);
static_assert(checked_numeric::add(2, 3, "test") == 5);
static_assert(checked_numeric::subtract(5, 3, "test") == 2);
static_assert(checked_numeric::multiply(4, 5, "test") == 20);

static_assert(byte_count::from_integer(4).value() == 4);
static_assert(byte_count::from_integer(4).plus(byte_count::from_integer(3)).value() == 7);
static_assert(byte_count::from_integer(4).multiplied_by(3).value() == 12);
static_assert(byte_offset::from_integer(10).advanced_by(byte_count::from_integer(5)).value() == 15);
static_assert(byte_offset::from_integer(10).distance_to(byte_offset::from_integer(15)).value() ==
              5);
static_assert(xref_field_width::from_integer(8).value() == 8);

static_assert(!std::is_constructible_v<byte_count, std::size_t>);
static_assert(!std::is_constructible_v<byte_offset, std::size_t>);
static_assert(!std::is_constructible_v<xref_field_width, std::uint8_t>);

TEST_CASE("checked_numeric::narrow rejects out-of-range integers", "[util][checked]")
{
    REQUIRE(checked_numeric::narrow<std::uint8_t>(255) == 255);
    REQUIRE_THROWS_AS(checked_numeric::narrow<std::uint8_t>(256), parse_exception);
    REQUIRE_THROWS_AS(checked_numeric::narrow<std::uint16_t>(-1), parse_exception);
    REQUIRE_THROWS_AS(checked_numeric::narrow<std::int8_t>(128), parse_exception);
    REQUIRE_THROWS_AS(checked_numeric::narrow<std::int8_t>(-129), parse_exception);
}

TEST_CASE("checked arithmetic rejects overflow and underflow", "[util][checked]")
{
    REQUIRE_THROWS_AS(checked_numeric::add(max_size, 1, "test"), parse_exception);
    REQUIRE_THROWS_AS(checked_numeric::subtract(0, 1, "test"), parse_exception);
    REQUIRE_THROWS_AS(checked_numeric::multiply(max_size, 2, "test"), parse_exception);
}

TEST_CASE("byte_count preserves its invariant through arithmetic", "[util][byte_count]")
{
    REQUIRE(byte_count::zero().value() == 0);
    REQUIRE(byte_count::from_integer(10).minus(byte_count::from_integer(3)).value() == 7);

    REQUIRE_THROWS_AS(byte_count::from_integer(-1), parse_exception);
    REQUIRE_THROWS_AS(byte_count::from_integer(3).minus(byte_count::from_integer(4)),
                      parse_exception);
    REQUIRE_THROWS_AS(byte_count::from_integer(3).plus(byte_count::from_integer(max_size)),
                      parse_exception);
    REQUIRE_THROWS_AS(byte_count::from_integer(3).multiplied_by(-1), parse_exception);
    REQUIRE_THROWS_AS(byte_count::from_integer(max_size).multiplied_by(2), parse_exception);
}

TEST_CASE("byte_offset keeps offsets and distances distinct", "[util][byte_offset]")
{
    const auto start = byte_offset::from_integer(100);
    const auto end = start.advanced_by(byte_count::from_integer(25));

    REQUIRE(end.value() == 125);
    REQUIRE(start.distance_to(end).value() == 25);
    REQUIRE(start < end);

    REQUIRE_THROWS_AS(byte_offset::from_integer(-1), parse_exception);
    REQUIRE_THROWS_AS(byte_offset::from_integer(max_size).advanced_by(byte_count::from_integer(1)),
                      parse_exception);
    REQUIRE_THROWS_AS(end.distance_to(start), parse_exception);
}

TEST_CASE("checked_subspan validates ranges against the buffer", "[util][subspan]")
{
    const std::array<std::byte, 4> data{};

    const auto middle =
        checked_subspan(data, byte_offset::from_integer(1), byte_count::from_integer(2));
    REQUIRE(middle.size() == 2);

    const auto at_end = checked_subspan(data, byte_offset::from_integer(4), byte_count::zero());
    REQUIRE(at_end.empty());

    REQUIRE_THROWS_AS(checked_subspan(data, byte_offset::from_integer(5), byte_count::zero()),
                      parse_exception);
    REQUIRE_THROWS_AS(
        checked_subspan(data, byte_offset::from_integer(3), byte_count::from_integer(2)),
        parse_exception);
}

TEST_CASE("xref_field_width only accepts 0 through 8", "[parser][xref_width]")
{
    for (std::uint8_t width = 0; width <= xref_field_width::max_width; ++width)
        REQUIRE(xref_field_width::from_integer(width).value() == width);

    REQUIRE_THROWS_AS(xref_field_width::from_integer(9), parse_exception);
    REQUIRE_THROWS_AS(xref_field_width::from_integer(-1), parse_exception);
    REQUIRE_THROWS_AS(xref_field_width::from_integer(300), parse_exception);
}
} // namespace ripper::pdf::core
