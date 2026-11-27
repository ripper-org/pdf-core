#include "ripper/io/core/reader/memory_reader.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/resource/font_resources.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> make_bytes(std::string_view text)
{
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = std::byte{static_cast<unsigned char>(text[i])};
    return bytes;
}

class pdf_builder
{
public:
    void add_object(std::string body)
    {
        offsets_.push_back(text_.size());
        text_ += std::move(body);
    }

    [[nodiscard]] std::vector<std::byte> finish()
    {
        const auto xref_offset = text_.size();

        text_ += "xref\n0 " + std::to_string(offsets_.size() + 1) + "\n";
        text_ += "0000000000 65535 f \n";

        for (const auto offset : offsets_)
        {
            std::ostringstream entry;
            entry << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
            text_ += entry.str();
        }

        text_ += "trailer\n<< /Size " + std::to_string(offsets_.size() + 1) + " /Root 1 0 R >>\n";
        text_ += "startxref\n" + std::to_string(xref_offset) + "\n%%EOF\n";

        return make_bytes(text_);
    }

private:
    std::string text_ = "%PDF-1.7\n";
    std::vector<std::size_t> offsets_;
};

/// Builds a one-page document with the given /Resources value; `objects` are
/// appended verbatim starting at object 4.
std::vector<std::byte> make_pdf(std::string_view resources_entry,
                                const std::vector<std::string>& objects)
{
    pdf_builder builder;

    builder.add_object("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    builder.add_object("2 0 obj\n<< /Type /Pages /Count 1 /Kids [3 0 R] >>\nendobj\n");
    builder.add_object("3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100]" +
                       std::string{resources_entry} + " >>\nendobj\n");

    for (const auto& body : objects)
        builder.add_object(body);

    return builder.finish();
}

std::vector<std::byte> make_font_pdf()
{
    return make_pdf(" /Resources 4 0 R",
                    {"4 0 obj\n<< /Font << /F1 5 0 R >> /XObject << /Im1 6 0 R >> >>\nendobj\n",
                     "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n",
                     "6 0 obj\n<< /Type /XObject /Subtype /Form /BBox [0 0 1 1] >>\nendobj\n"});
}

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

    [[nodiscard]] document& doc()
    {
        return doc_;
    }

private:
    test_fixture::scoped_temp_file file_{"pdf_ripper_core_page_fonts_test.pdf"};
    document doc_;
};
} // namespace

TEST_CASE("page fonts expose registered fonts", "[page_fonts]")
{
    auto data = make_font_pdf();
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto fonts = candidate->resources().fonts();

    REQUIRE_FALSE(fonts.empty());
    REQUIRE(fonts.has("F1"));
    REQUIRE_FALSE(fonts.has("F2"));

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);
    REQUIRE(entry->is_indirect_reference());
    REQUIRE(fonts.find("F2") == nullptr);

    auto* font = doc.resolve_object(*entry->as_indirect_reference());
    REQUIRE(font != nullptr);

    const auto* dictionary = font->content().as_dictionary();
    REQUIRE(dictionary != nullptr);
    REQUIRE(dictionary->get_name("BaseFont")->value == "Helvetica");
}

TEST_CASE("page fonts generate unique names", "[page_fonts]")
{
    auto data = make_font_pdf();
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto fonts = candidate->resources().fonts();

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);

    auto* existing = doc.resolve_object(*entry->as_indirect_reference());
    REQUIRE(existing != nullptr);

    REQUIRE(fonts.add(*existing) == "F2");
    REQUIRE(fonts.add(*existing) == "F3");

    REQUIRE(fonts.has("F2"));
    REQUIRE(fonts.has("F3"));

    const auto* third = fonts.find("F3");
    REQUIRE(third != nullptr);
    REQUIRE(*third->as_indirect_reference() == existing->identity().reference());
}

TEST_CASE("page fonts set, remove and clear entries", "[page_fonts]")
{
    auto data = make_font_pdf();
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto fonts = candidate->resources().fonts();

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);

    auto* existing = doc.resolve_object(*entry->as_indirect_reference());
    REQUIRE(existing != nullptr);

    fonts.set("Custom", *existing);
    REQUIRE(fonts.has("Custom"));

    fonts.remove("F1");
    REQUIRE_FALSE(fonts.has("F1"));

    fonts.remove("Missing");
    REQUIRE(fonts.has("Custom"));

    fonts.clear();
    REQUIRE(fonts.empty());
    REQUIRE_FALSE(fonts.has("Custom"));

    // Clearing /Font leaves other categories untouched.
    const auto* resources = candidate->obj().content().as_dictionary()->get_dictionary("Resources");
    REQUIRE(resources != nullptr);
    REQUIRE(resources->get_dictionary("XObject")->contains("Im1"));
}

TEST_CASE("page fonts reject a malformed /Font category", "[page_fonts]")
{
    auto data = make_pdf(" /Resources 4 0 R", {"4 0 obj\n<< /Font 42 >>\nendobj\n"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE_THROWS_AS(resources.fonts(), logic_exception);
}

TEST_CASE("page fonts register a font and feed the brush", "[page_fonts][write]")
{
    writer_document fixture;
    auto target = fixture.first_page();

    auto fonts = target.resources().fonts();
    REQUIRE(fonts.empty());

    auto& font = object_manager::allocate_object(fixture.doc(), object{dictionary_object{}});

    const auto name = fonts.add(font);
    REQUIRE(name == "F1");
    REQUIRE(fonts.has("F1"));

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);
    REQUIRE(*entry->as_indirect_reference() == font.identity().reference());

    auto painter = target.brush(graphics::brush_mode::append);
    painter.text().set_font(name, 12.0).show("Hi").end();

    auto contents = target.content();
    REQUIRE(contents.size() == 1);
    REQUIRE(content_of(*contents.at(0).stream()) == "BT\n/F1 12.0 Tf\n(Hi) Tj\nET\n");
}
} // namespace ripper::pdf::core
