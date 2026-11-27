#include "ripper/io/core/reader/memory_reader.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/resource/page_resources.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

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

/// Builds a one-page document with extra entries on the /Pages node and the
/// /Page leaf; `objects` are appended verbatim starting at object 4.
std::vector<std::byte> make_pdf(std::string_view pages_entries, std::string_view page_entries,
                                const std::vector<std::string>& objects)
{
    pdf_builder builder;

    builder.add_object("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    builder.add_object("2 0 obj\n<< /Type /Pages /Count 1 /Kids [3 0 R]" +
                       std::string{pages_entries} + " >>\nendobj\n");
    builder.add_object("3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100]" +
                       std::string{page_entries} + " >>\nendobj\n");

    for (const auto& body : objects)
        builder.add_object(body);

    return builder.finish();
}
} // namespace

TEST_CASE("page without /Resources gets an empty dictionary", "[page_resources]")
{
    auto data = make_pdf("", "", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE(resources.empty());
    REQUIRE(resources.fonts().empty());

    const auto* entry = candidate->obj().content().as_dictionary()->get("Resources");
    REQUIRE(entry != nullptr);
    REQUIRE(entry->is_dictionary());
    REQUIRE(entry->as_dictionary()->get_dictionary("Font") != nullptr);
}

TEST_CASE("page with null /Resources gets an empty dictionary", "[page_resources]")
{
    auto data = make_pdf("", " /Resources null", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE(resources.empty());
    REQUIRE(resources.fonts().empty());
}

TEST_CASE("page copies inherited /Resources into its own dictionary", "[page_resources]")
{
    auto data =
        make_pdf(" /Resources 4 0 R", "",
                 {"4 0 obj\n<< /Font << /F1 5 0 R >> >>\nendobj\n",
                  "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE_FALSE(resources.empty());

    auto fonts = resources.fonts();
    REQUIRE(fonts.has("F1"));

    const auto* entry = fonts.find("F1");
    REQUIRE(entry != nullptr);

    auto* existing = doc.resolve_object(*entry->as_indirect_reference());
    REQUIRE(existing != nullptr);

    REQUIRE(fonts.add(*existing) == "F2");

    // The ancestor keeps only its original entry.
    auto* ancestor = doc.resolve_object(indirect_reference{4, 0});
    REQUIRE(ancestor != nullptr);

    const auto* ancestor_resources = ancestor->content().as_dictionary();
    REQUIRE(ancestor_resources != nullptr);

    const auto* ancestor_fonts = ancestor_resources->get_dictionary("Font");
    REQUIRE(ancestor_fonts != nullptr);
    REQUIRE(ancestor_fonts->contains("F1"));
    REQUIRE_FALSE(ancestor_fonts->contains("F2"));
}

TEST_CASE("page normalizes an indirect /Resources entry", "[page_resources]")
{
    auto data =
        make_pdf("", " /Resources 4 0 R",
                 {"4 0 obj\n<< /Font << /F1 5 0 R >> >>\nendobj\n",
                  "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE(resources.fonts().has("F1"));

    const auto* page = candidate->obj().content().as_dictionary();
    REQUIRE(page != nullptr);

    const auto* entry = page->get("Resources");
    REQUIRE(entry != nullptr);
    REQUIRE(entry->is_dictionary());
    REQUIRE(entry->as_dictionary()->get_dictionary("Font")->contains("F1"));
}

TEST_CASE("page normalizes an indirect /Font entry", "[page_resources]")
{
    auto data =
        make_pdf("", " /Resources 4 0 R",
                 {"4 0 obj\n<< /Font 5 0 R >>\nendobj\n", "5 0 obj\n<< /F1 6 0 R >>\nendobj\n",
                  "6 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto resources = candidate->resources();
    REQUIRE(resources.fonts().has("F1"));

    const auto* page = candidate->obj().content().as_dictionary();
    REQUIRE(page != nullptr);

    const auto* resources_entry = page->get("Resources");
    REQUIRE(resources_entry != nullptr);
    REQUIRE(resources_entry->is_dictionary());

    const auto* font_entry = resources_entry->as_dictionary()->get("Font");
    REQUIRE(font_entry != nullptr);
    REQUIRE(font_entry->is_dictionary());
    REQUIRE(font_entry->as_dictionary()->contains("F1"));
}

TEST_CASE("page rejects a malformed /Resources", "[page_resources]")
{
    SECTION("non-dictionary /Resources")
    {
        auto data = make_pdf("", " /Resources 42", {});
        document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

        auto candidate = doc.catalog().pages().page(0);
        REQUIRE(candidate.has_value());

        auto resources = candidate->resources();
        REQUIRE_THROWS_AS(resources.empty(), logic_exception);
    }

    SECTION("non-dictionary inherited /Resources")
    {
        auto data = make_pdf(" /Resources 42", "", {});
        document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

        auto candidate = doc.catalog().pages().page(0);
        REQUIRE(candidate.has_value());

        REQUIRE_THROWS_AS(candidate->resources(), logic_exception);
    }
}
} // namespace ripper::pdf::core
