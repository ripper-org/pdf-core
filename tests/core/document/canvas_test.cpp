#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/canvas/path.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace ripper::pdf::core
{
namespace
{
std::string content_of(const stream_object& stream)
{
    const auto& bytes = stream.stream().data();
    std::string text;
    text.reserve(bytes.size());

    for (const std::byte byte : bytes)
        text.push_back(static_cast<char>(byte));

    return text;
}

class writer_document
{
public:
    writer_document() : doc_{document::create(file_.path())} {}

    [[nodiscard]] page first_page()
    {
        auto pages = doc_.catalog().pages();
        (void)pages.append_page();

        auto added = pages.page(0);
        REQUIRE(added.has_value());
        return *added;
    }

private:
    test_fixture::scoped_temp_file file_{"pdf_ripper_core_canvas_test.pdf"};
    document doc_;
};
} // namespace

TEST_CASE("canvas writes operators in operand-then-operator order", "[canvas]")
{
    stream_object stream;

    canvas{stream}.op("cm", {object{std::int64_t{1}}, object{std::int64_t{0}},
                             object{std::int64_t{0}}, object{std::int64_t{1}},
                             object{std::int64_t{10}}, object{std::int64_t{20}}});

    REQUIRE(content_of(stream) == "1 0 0 1 10 20 cm\n");
    REQUIRE(stream.dictionary().get_number("Length")->as_integer() ==
            static_cast<std::int64_t>(content_of(stream).size()));
}

TEST_CASE("canvas tracks graphics state depth", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    REQUIRE(painter.state_depth() == 0);

    painter.save();
    REQUIRE(painter.state_depth() == 1);

    painter.restore();
    REQUIRE(painter.state_depth() == 0);

    REQUIRE_THROWS_AS(painter.restore(), logic_exception);
    REQUIRE(content_of(stream) == "q\nQ\n");
}

TEST_CASE("canvas emits transformation operators", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    painter.translate(5.0, 7.0);
    painter.scale(2.0, 3.0);
    painter.rotate(0.0);

    REQUIRE(content_of(stream) ==
            "1.0 0.0 0.0 1.0 5.0 7.0 cm\n2.0 0.0 0.0 3.0 0.0 0.0 cm\n1.0 0.0 0.0 1.0 0.0 0.0 cm\n");
}

TEST_CASE("page canvas appends a stream to /Contents", "[canvas][page][write]")
{
    writer_document fixture;
    auto target = fixture.first_page();

    auto painter = target.canvas(canvas_mode::append);
    painter.rotate(0.0);

    auto contents = target.content();
    REQUIRE(contents.size() == 1);
    REQUIRE_FALSE(contents.is_array());

    auto streams = contents.streams();
    REQUIRE(streams.size() == 1);
    REQUIRE(content_of(*streams.front().stream()) == "1.0 0.0 0.0 1.0 0.0 0.0 cm\n");
}

TEST_CASE("page canvas prepend and replace order the content streams", "[canvas][page][write]")
{
    writer_document fixture;
    auto target = fixture.first_page();

    target.canvas(canvas_mode::append).translate(1.0, 1.0);
    target.canvas(canvas_mode::prepend).translate(2.0, 2.0);

    auto contents = target.content();
    REQUIRE(contents.is_array());
    REQUIRE(contents.size() == 2);
    REQUIRE(content_of(*contents.at(0).stream()) == "1.0 0.0 0.0 1.0 2.0 2.0 cm\n");
    REQUIRE(content_of(*contents.at(1).stream()) == "1.0 0.0 0.0 1.0 1.0 1.0 cm\n");

    target.canvas(canvas_mode::replace).translate(3.0, 3.0);

    auto replaced = target.content();
    REQUIRE(replaced.size() == 1);
    REQUIRE_FALSE(replaced.is_array());
    REQUIRE(content_of(*replaced.at(0).stream()) == "1.0 0.0 0.0 1.0 3.0 3.0 cm\n");
}

TEST_CASE("canvas chains mutating calls", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    painter.save()
        .translate(1.0, 2.0)
        .set_line_width(2.0)
        .set_line_cap(line_cap::round)
        .set_line_join(line_join::bevel)
        .set_miter_limit(4.0)
        .path()
        .move_to({0.0, 0.0})
        .line_to({10.0, 0.0})
        .close()
        .stroke()
        .restore();

    REQUIRE(content_of(stream) == "q\n"
                                  "1.0 0.0 0.0 1.0 1.0 2.0 cm\n"
                                  "2.0 w\n"
                                  "1 J\n"
                                  "2 j\n"
                                  "4.0 M\n"
                                  "0.0 0.0 m\n"
                                  "10.0 0.0 l\n"
                                  "h\n"
                                  "S\n"
                                  "Q\n");
    REQUIRE(painter.state_depth() == 0);
}

TEST_CASE("canvas emits line state operators", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    painter.set_line_width(0.5)
        .set_line_cap(line_cap::square)
        .set_line_join(line_join::miter)
        .set_miter_limit(3.0)
        .set_dash_pattern({3.0, 1.0}, 0.0);

    REQUIRE(content_of(stream) == "0.5 w\n2 J\n0 j\n3.0 M\n[3.0 1.0] 0.0 d\n");
}

TEST_CASE("canvas rejects invalid line state", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    REQUIRE_THROWS_AS(painter.set_line_width(-1.0), logic_exception);
    REQUIRE_THROWS_AS(painter.set_miter_limit(0.5), logic_exception);
    REQUIRE_THROWS_AS(painter.set_dash_pattern({0.0, 0.0}), logic_exception);
    REQUIRE_THROWS_AS(painter.set_dash_pattern({1.0, -1.0}), logic_exception);
    REQUIRE(content_of(stream).empty());
}

TEST_CASE("path builds and fills rectangles", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    painter.fill_rect(geometry::rect{10.0, 20.0, 30.0, 40.0});
    painter.path().add_rect(geometry::rect{1.0, 2.0, 3.0, 4.0}).fill(fill_rule::even_odd);

    REQUIRE(content_of(stream) == "10.0 20.0 30.0 40.0 re\nf\n1.0 2.0 3.0 4.0 re\nf*\n");
}

TEST_CASE("path emits curve operators", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

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

TEST_CASE("path maps paint styles to operators", "[canvas][path]")
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
        canvas painter{stream};

        painter.path().move_to({0.0, 0.0}).paint(style);

        REQUIRE(content_of(stream) == "0.0 0.0 m\n" + std::string{expected} + "\n");
    }
}

TEST_CASE("path clips and discards paths", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    painter.path().add_rect({0.0, 0.0, 10.0, 10.0}).clip();
    painter.path().add_rect({1.0, 1.0, 2.0, 2.0}).clip(fill_rule::even_odd);
    painter.path().move_to({0.0, 0.0}).line_to({1.0, 1.0}).discard();

    REQUIRE(content_of(stream) == "0.0 0.0 10.0 10.0 re\nW\nn\n"
                                  "1.0 1.0 2.0 2.0 re\nW*\nn\n"
                                  "0.0 0.0 m\n1.0 1.0 l\nn\n");
}

TEST_CASE("path rejects segments without an open path", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().curve_to({1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}), logic_exception);
    REQUIRE_THROWS_AS(painter.path().close(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().stroke(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().clip(), logic_exception);
    REQUIRE_THROWS_AS(painter.path().discard(), logic_exception);
    REQUIRE(content_of(stream).empty());
}

TEST_CASE("path views do not share path state", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    auto first = painter.path();
    first.move_to({0.0, 0.0});

    // A fresh view starts with no open path even though the stream has one.
    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);

    first.stroke();
    REQUIRE(content_of(stream) == "0.0 0.0 m\nS\n");
}

TEST_CASE("path ignores path operators emitted via op", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    painter.op("m", {object{std::int64_t{0}}, object{std::int64_t{0}}});

    REQUIRE_THROWS_AS(painter.path().line_to({1.0, 1.0}), logic_exception);
    REQUIRE(content_of(stream) == "0 0 m\n");
}

TEST_CASE("a path view can start more than one path", "[canvas][path]")
{
    stream_object stream;
    canvas painter{stream};

    auto target = painter.path();
    target.move_to({0.0, 0.0}).line_to({1.0, 1.0}).stroke();
    target.move_to({2.0, 2.0}).line_to({3.0, 3.0}).stroke();

    REQUIRE(content_of(stream) == "0.0 0.0 m\n1.0 1.0 l\nS\n2.0 2.0 m\n3.0 3.0 l\nS\n");
}

TEST_CASE("canvas and path are non-copyable views", "[canvas][path]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<canvas>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<canvas>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<path>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<path>);
}

TEST_CASE("geometry rect normalizes corners", "[geometry]")
{
    const auto normalized =
        geometry::rect::from_corners(geometry::point{10.0, 20.0}, geometry::point{4.0, 5.0});

    REQUIRE(normalized == geometry::rect{4.0, 5.0, 6.0, 15.0});
}

} // namespace ripper::pdf::core
