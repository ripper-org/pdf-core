#include "ripper/pdf/core/document/geometry/matrix.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/document/graphics/text/text.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>

namespace ripper::pdf::core::graphics
{
using test_support::content_of;

TEST_CASE("brush emits text state operators", "[brush][text]")
{
    stream_object stream;
    brush painter{stream};

    painter.set_font("F1", 12.0)
        .set_character_spacing(0.5)
        .set_word_spacing(1.0)
        .set_horizontal_scaling(90.0)
        .set_leading(14.0)
        .set_text_rise(2.0)
        .set_text_render_mode(text_render_mode::fill_stroke);

    REQUIRE(content_of(stream) == "/F1 12.0 Tf\n"
                                  "0.5 Tc\n"
                                  "1.0 Tw\n"
                                  "90.0 Tz\n"
                                  "14.0 TL\n"
                                  "2.0 Ts\n"
                                  "2 Tr\n");
}

TEST_CASE("text object emits positioning and showing operators", "[brush][text]")
{
    stream_object stream;
    brush painter{stream};

    auto& ended = painter.text()
                      .set_font("F1", 12.0)
                      .move_by(10.0, 20.0)
                      .show("Hello")
                      .next_line()
                      .show_next_line("World")
                      .move_by_and_set_leading(0.0, -14.0)
                      .set_matrix(geometry::matrix::translated(1.0, 2.0))
                      .end();

    REQUIRE(&ended == &painter);
    REQUIRE_FALSE(painter.in_text());
    REQUIRE(content_of(stream) == "BT\n"
                                  "/F1 12.0 Tf\n"
                                  "10.0 20.0 Td\n"
                                  "(Hello) Tj\n"
                                  "T*\n"
                                  "(World) '\n"
                                  "0.0 -14.0 TD\n"
                                  "1.0 0.0 0.0 1.0 1.0 2.0 Tm\n"
                                  "ET\n");
}

TEST_CASE("text object emits quote and array showing operators", "[brush][text]")
{
    stream_object stream;
    brush painter{stream};

    painter.text().show_next_line(1.0, 2.0, "X").end();

    REQUIRE(content_of(stream) == "BT\n1.0 2.0 (X) \"\nET\n");

    painter.text().show({std::string{"A"}, -120.0, std::string{"V"}}).end();

    REQUIRE(content_of(stream) == "BT\n1.0 2.0 (X) \"\nET\n"
                                  "BT\n[(A) -120.0 (V)] TJ\nET\n");
}

TEST_CASE("text objects cannot nest or be reused", "[brush][text]")
{
    stream_object stream;
    brush painter{stream};

    auto first = painter.text();

    REQUIRE(painter.in_text());
    REQUIRE_THROWS_AS(painter.text(), logic_exception);

    first.show("x");
    first.end();

    REQUIRE_FALSE(painter.in_text());
    REQUIRE_THROWS_AS(first.end(), logic_exception);
    REQUIRE_THROWS_AS(first.show("y"), logic_exception);
    REQUIRE(content_of(stream) == "BT\n(x) Tj\nET\n");
}

TEST_CASE("text is a non-copyable view", "[brush][text]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<text>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<text>);
}
} // namespace ripper::pdf::core::graphics
