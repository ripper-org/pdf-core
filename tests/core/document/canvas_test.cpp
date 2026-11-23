#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

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
        (void)pages.add_page();

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

    REQUIRE(
        content_of(stream) ==
        "1.0 0.0 0.0 1.0 5.0 7.0 cm\n2.0 0.0 0.0 3.0 0.0 0.0 cm\n1.0 0.0 -0.0 1.0 0.0 0.0 cm\n");
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
    REQUIRE(content_of(*streams.front().stream()) == "1.0 0.0 -0.0 1.0 0.0 0.0 cm\n");
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

} // namespace ripper::pdf::core
