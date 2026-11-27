#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/resource/font_resources.hpp"
#include "ripper/pdf/core/document/font/font.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>
#include <string_view>

namespace ripper::pdf::core
{
namespace
{
std::string decoded_text(const stream_object& source)
{
    const auto decoded = source.decode();
    const auto bytes = decoded.bytes();

    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
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

    [[nodiscard]] document& doc()
    {
        return doc_;
    }

private:
    test_fixture::scoped_temp_file file_{"pdf_ripper_core_font_test.pdf"};
    document doc_;
};
} // namespace

TEST_CASE("standard font names match the standard-14 base fonts", "[font]")
{
    REQUIRE(standard_font_name(standard_font::helvetica) == "Helvetica");
    REQUIRE(standard_font_name(standard_font::helvetica_bold_oblique) == "Helvetica-BoldOblique");
    REQUIRE(standard_font_name(standard_font::times_roman) == "Times-Roman");
    REQUIRE(standard_font_name(standard_font::courier_bold) == "Courier-Bold");
    REQUIRE(standard_font_name(standard_font::symbol) == "Symbol");
    REQUIRE(standard_font_name(standard_font::zapf_dingbats) == "ZapfDingbats");
}

TEST_CASE("create_standard builds a Type 1 font dictionary", "[font]")
{
    writer_document fixture;

    auto helvetica = font::create_standard(fixture.doc(), standard_font::helvetica);

    REQUIRE(helvetica.subtype() == "Type1");
    REQUIRE(helvetica.base_font() == "Helvetica");
    REQUIRE(helvetica.encoding() == "WinAnsiEncoding");
    REQUIRE_FALSE(helvetica.is_composite());

    const auto& dictionary = helvetica.dictionary();
    REQUIRE(dictionary.get_name("Type")->value == "Font");

    auto symbol = font::create_standard(fixture.doc(), standard_font::symbol);
    REQUIRE(symbol.base_font() == "Symbol");
    REQUIRE(symbol.encoding().empty());
}

TEST_CASE("font_resources registers a typed font and feeds the brush", "[font][page_fonts][write]")
{
    writer_document fixture;
    auto target = fixture.first_page();

    auto helvetica = font::create_standard(fixture.doc(), standard_font::helvetica);
    auto fonts = target.resources().fonts();

    const auto name = fonts.add(helvetica);
    REQUIRE(name == "F1");
    REQUIRE(fonts.has(name));

    const auto* entry = fonts.find(name);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->is_indirect_reference());
    REQUIRE(*entry->as_indirect_reference() == helvetica.obj().identity().reference());

    auto painter = target.brush(graphics::brush_mode::append);
    painter.text().set_font(name, 12.0).show("Hi").end();

    auto contents = target.content();
    REQUIRE(contents.size() == 1);
    REQUIRE(decoded_text(*contents.at(0).stream()) == "BT\n/F1 12.0 Tf\n(Hi) Tj\nET\n");
}

TEST_CASE("a standard font survives save and reopen", "[font][e2e][save]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_standard_font_e2e_test.pdf"};

    {
        auto doc = document::create(output.path());

        auto pages = doc.catalog().pages();
        (void)pages.append_page();

        auto target = pages.page(0);
        REQUIRE(target.has_value());

        auto helvetica = font::create_standard(doc, standard_font::helvetica);
        const auto name = target->resources().fonts().add(helvetica);

        target->brush(graphics::brush_mode::append).text().set_font(name, 11.0).show("Hello").end();

        REQUIRE_NOTHROW(doc.save());
    }

    auto read_back = document::open(output.path());

    auto target = read_back.catalog().pages().page(0);
    REQUIRE(target.has_value());

    auto fonts = target->resources().fonts();
    REQUIRE(fonts.has("F1"));

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);

    auto* font_object = read_back.resolve_object(*entry->as_indirect_reference());
    REQUIRE(font_object != nullptr);

    font loaded{*font_object};
    REQUIRE(loaded.base_font() == "Helvetica");
    REQUIRE(loaded.subtype() == "Type1");

    auto contents = target->content();
    REQUIRE(contents.size() == 1);
    REQUIRE(decoded_text(*contents.at(0).stream()) == "BT\n/F1 11.0 Tf\n(Hello) Tj\nET\n");
}
} // namespace ripper::pdf::core
