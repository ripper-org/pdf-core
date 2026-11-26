#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/graphics/color.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_fixture.hpp"
#include "test_support.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace ripper::pdf::core
{
using test_support::content_of;

namespace
{
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
    REQUIRE(contents.is_array());

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
    REQUIRE(replaced.is_array());
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

TEST_CASE("canvas emits device color operators", "[canvas][color]")
{
    stream_object stream;
    canvas painter{stream};

    painter.set_fill_color(color::gray(0.5))
        .set_stroke_color(color::rgb(1.0, 0.0, 0.0))
        .set_fill_color(color::cmyk(0.0, 0.0, 0.0, 1.0))
        .set_stroke_color(color::gray(0.25));

    REQUIRE(content_of(stream) == "0.5 g\n1.0 0.0 0.0 RG\n0.0 0.0 0.0 1.0 k\n0.25 G\n");
}

TEST_CASE("canvas emits color space and pattern operators", "[canvas][color]")
{
    stream_object stream;
    canvas painter{stream};

    painter.set_fill_color_space("Pattern")
        .set_stroke_color_space("DeviceCMYK")
        .set_fill_color(std::array{0.5})
        .set_stroke_color(std::array{0.1, 0.2, 0.3, 0.4})
        .set_fill_color(std::array{0.5}, "P0")
        .set_stroke_color(std::array{0.25}, "P1");

    REQUIRE(content_of(stream) == "/Pattern cs\n"
                                  "/DeviceCMYK CS\n"
                                  "0.5 sc\n"
                                  "0.1 0.2 0.3 0.4 SC\n"
                                  "0.5 /P0 scn\n"
                                  "0.25 /P1 SCN\n");
}

TEST_CASE("canvas emits graphics state and named resource operators", "[canvas][graphics]")
{
    stream_object stream;
    canvas painter{stream};

    painter.set_rendering_intent(rendering_intent::relative_colorimetric)
        .set_flatness(0.5)
        .set_graphics_state("GS0")
        .draw_xobject("Im1")
        .paint_shading("Sh0");

    REQUIRE(content_of(stream) == "/RelativeColorimetric ri\n"
                                  "0.5 i\n"
                                  "/GS0 gs\n"
                                  "/Im1 Do\n"
                                  "/Sh0 sh\n");

    REQUIRE_THROWS_AS(painter.set_flatness(-1.0), logic_exception);
    REQUIRE_THROWS_AS(painter.set_flatness(101.0), logic_exception);
}

TEST_CASE("canvas emits marked content and compatibility operators", "[canvas][marked]")
{
    stream_object stream;
    canvas painter{stream};

    painter.mark_point("Tag")
        .begin_marked_content("Span")
        .end_marked_content()
        .begin_marked_content("Artifact", "MC0")
        .end_marked_content()
        .begin_compatibility()
        .end_compatibility();

    REQUIRE(painter.marked_content_depth() == 0);
    REQUIRE(painter.compatibility_depth() == 0);
    REQUIRE(content_of(stream) == "/Tag MP\n"
                                  "/Span BMC\n"
                                  "EMC\n"
                                  "/Artifact /MC0 BDC\n"
                                  "EMC\n"
                                  "BX\n"
                                  "EX\n");

    dictionary_object properties;
    properties.set("ActualText", object{std::string{"x"}});

    painter.mark_point("Tag", properties).begin_marked_content("Span", properties);

    REQUIRE(content_of(stream) == "/Tag MP\n"
                                  "/Span BMC\n"
                                  "EMC\n"
                                  "/Artifact /MC0 BDC\n"
                                  "EMC\n"
                                  "BX\n"
                                  "EX\n"
                                  "/Tag <<\n/ActualText (x)\n>> DP\n"
                                  "/Span <<\n/ActualText (x)\n>> BDC\n");
}

TEST_CASE("canvas rejects unbalanced marked content and compatibility", "[canvas][marked]")
{
    stream_object stream;
    canvas painter{stream};

    REQUIRE_THROWS_AS(painter.end_marked_content(), logic_exception);
    REQUIRE_THROWS_AS(painter.end_compatibility(), logic_exception);
    REQUIRE(content_of(stream).empty());
}

TEST_CASE("canvas draws inline images", "[canvas]")
{
    stream_object stream;
    canvas painter{stream};

    inline_image image;
    image.dictionary.set("W", object{std::int64_t{1}});
    image.dictionary.set("H", object{std::int64_t{1}});
    image.data = {std::byte{0x01}, std::byte{0x02}};

    painter.draw_image(image);

    const auto content = content_of(stream);
    REQUIRE(content.starts_with("BI\n"));
    REQUIRE(content.find("/W 1\n") != std::string::npos);
    REQUIRE(content.find("/H 1\n") != std::string::npos);
    REQUIRE(content.find("ID \x01\x02") != std::string::npos);
    REQUIRE(content.ends_with("\nEI\n"));
}

TEST_CASE("canvas is a non-copyable view", "[canvas]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<canvas>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<canvas>);
}
} // namespace ripper::pdf::core
