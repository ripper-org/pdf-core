#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/document/graphics/path/path.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace ripper::pdf::core::graphics
{
using test_support::content_of;
using test_support::count_of;

TEST_CASE("path builds and fills rectangles", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.fill_rect(geometry::rect{10.0, 20.0, 30.0, 40.0});
    painter.path().add_rect(geometry::rect{1.0, 2.0, 3.0, 4.0}).fill(fill_rule::even_odd);

    REQUIRE(content_of(stream) == "10.0 20.0 30.0 40.0 re\nf\n1.0 2.0 3.0 4.0 re\nf*\n");
}

TEST_CASE("path emits curve operators", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path()
        .move_to({1.0, 2.0})
        .curve_to({3.0, 4.0}, {5.0, 6.0}, {7.0, 8.0})
        .curve_to_replicated_start({9.0, 10.0}, {11.0, 12.0})
        .curve_to_replicated_end({13.0, 14.0}, {15.0, 16.0})
        .close()
        .stroke();

    REQUIRE(content_of(stream) == "1.0 2.0 m\n"
                                  "3.0 4.0 5.0 6.0 7.0 8.0 c\n"
                                  "9.0 10.0 11.0 12.0 v\n"
                                  "13.0 14.0 15.0 16.0 y\n"
                                  "h\n"
                                  "S\n");
}

TEST_CASE("path maps paint styles to operators", "[brush][path]")
{
    struct paint_case
    {
        paint_style style;
        std::string_view expected;
    };

    const paint_case cases[] = {
        {paint_style::stroke, "S"},
        {paint_style::close_stroke, "s"},
        {paint_style::fill, "f"},
        {paint_style::fill_even_odd, "f*"},
        {paint_style::fill_stroke, "B"},
        {paint_style::fill_stroke_even_odd, "B*"},
        {paint_style::close_fill_stroke, "b"},
        {paint_style::close_fill_stroke_even_odd, "b*"},
    };

    for (const auto& [style, expected] : cases)
    {
        stream_object stream;
        brush painter{stream};

        painter.path().move_to({0.0, 0.0}).paint(style);

        REQUIRE(content_of(stream) == "0.0 0.0 m\n" + std::string{expected} + "\n");
    }
}

TEST_CASE("path clips and discards paths", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path().add_rect({0.0, 0.0, 10.0, 10.0}).clip();
    painter.path().add_rect({1.0, 1.0, 2.0, 2.0}).clip(fill_rule::even_odd);
    painter.path().move_to({0.0, 0.0}).line_to({1.0, 1.0}).discard();

    REQUIRE(content_of(stream) == "0.0 0.0 10.0 10.0 re\nW\nn\n"
                                  "1.0 1.0 2.0 2.0 re\nW*\nn\n"
                                  "0.0 0.0 m\n1.0 1.0 l\nn\n");
}

TEST_CASE("path rejects segments without an open path", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().curve_to({1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().close(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().stroke(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().clip(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().discard(), logic_exception);
    REQUIRE(content_of(stream).empty());
}

TEST_CASE("path views do not share path state", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    auto first = painter.path();
    first.move_to({0.0, 0.0});

    // A fresh view starts with no open path even though the stream has one.
    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);

    first.stroke();
    REQUIRE(content_of(stream) == "0.0 0.0 m\nS\n");
}

TEST_CASE("path ignores path operators emitted via op", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.op("m", {object{std::int64_t{0}}, object{std::int64_t{0}}});

    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);
    REQUIRE(content_of(stream) == "0 0 m\n");
}

TEST_CASE("a path view can start more than one path", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    auto target = painter.path();
    target.move_to({0.0, 0.0}).line_to({1.0, 1.0}).stroke();
    target.move_to({2.0, 2.0}).line_to({3.0, 3.0}).stroke();

    REQUIRE(content_of(stream) == "0.0 0.0 m\n1.0 1.0 l\nS\n2.0 2.0 m\n3.0 3.0 l\nS\n");
}

TEST_CASE("path adds polylines and polygons", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path().add_polyline({{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}}).stroke();

    REQUIRE(content_of(stream) == "0.0 0.0 m\n1.0 1.0 l\n2.0 2.0 l\nS\n");

    painter.path().add_polygon({{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}}).fill();

    REQUIRE(content_of(stream) == "0.0 0.0 m\n1.0 1.0 l\n2.0 2.0 l\nS\n"
                                  "0.0 0.0 m\n1.0 0.0 l\n0.0 1.0 l\nh\nf\n");
}

TEST_CASE("path approximates circles and ellipses with four curves", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path().add_circle({0.0, 0.0}, 1.0).stroke();

    const auto circle = content_of(stream);
    REQUIRE(circle.starts_with("1.0 0.0 m\n"));
    REQUIRE(circle.ends_with("h\nS\n"));
    REQUIRE(count_of(circle, " c\n") == 4);

    painter.path().add_ellipse({5.0, 5.0}, 2.0, 1.0).fill();

    const auto content = content_of(stream);
    REQUIRE(count_of(content, " c\n") == 8);
    REQUIRE(count_of(content, " m\n") == 2);
    REQUIRE(count_of(content, "\nh\n") == 2);
}

TEST_CASE("path adds rounded rectangles", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path().add_rounded_rect({0.0, 0.0, 10.0, 10.0}, 0.0).stroke();

    REQUIRE(content_of(stream) == "0.0 0.0 10.0 10.0 re\nS\n");

    painter.path().add_rounded_rect({0.0, 0.0, 10.0, 10.0}, 2.0).stroke();

    const auto content = content_of(stream);
    REQUIRE(content.find("0.0 0.0 10.0 10.0 re\nS\n2.0 0.0 m\n") != std::string::npos);
    REQUIRE(count_of(content, " c\n") == 4);
    REQUIRE(content.ends_with("h\nS\n"));
}

TEST_CASE("path adds elliptical arcs", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    painter.path().add_arc({0.0, 0.0}, 1.0, 0.0, 90.0).stroke();

    const auto quarter = content_of(stream);
    REQUIRE(quarter.starts_with("1.0 0.0 m\n"));
    REQUIRE(count_of(quarter, " c\n") == 1);
    REQUIRE(count_of(quarter, "\nh\n") == 0);

    painter.path().add_arc({0.0, 0.0}, 1.0, 0.0, 180.0).stroke();
    painter.path().add_arc({0.0, 0.0}, 1.0, 0.0, 360.0).stroke();
    painter.path().add_arc({0.0, 0.0}, 1.0, 0.0, -90.0).stroke();

    const auto content = content_of(stream);
    REQUIRE(count_of(content, " c\n") == 1 + 2 + 4 + 1);
}

TEST_CASE("path geometry rejects invalid input", "[brush][path]")
{
    stream_object stream;
    brush painter{stream};

    REQUIRE_THROWS_AS(painter.path().add_polyline({{0.0, 0.0}}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_polygon({{0.0, 0.0}, {1.0, 1.0}}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_circle({0.0, 0.0}, 0.0), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_ellipse({0.0, 0.0}, 1.0, -1.0), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_rounded_rect({0.0, 0.0, 1.0, 1.0}, -1.0), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_rounded_rect({0.0, 0.0, 0.0, 1.0}, 1.0), logic_exception);
    REQUIRE_THROWS_AS(painter.path().add_arc({0.0, 0.0}, 1.0, 0.0, 0.0), logic_exception);
    REQUIRE(content_of(stream).empty());
}

TEST_CASE("path is a non-copyable view", "[brush][path]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<path>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<path>);
}
} // namespace ripper::pdf::core::graphics
