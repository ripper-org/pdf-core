#include "ripper/io/core/reader/memory_reader.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
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

std::vector<std::byte> make_page_pdf(const std::string& contents_entry,
                                     const std::vector<std::string>& streams)
{
    pdf_builder builder;

    builder.add_object("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    builder.add_object("2 0 obj\n<< /Type /Pages /Count 1 /Kids [3 0 R] >>\nendobj\n");

    std::string page = "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100]";
    if (!contents_entry.empty())
        page += " /Contents " + contents_entry;
    page += " >>\nendobj\n";
    builder.add_object(std::move(page));

    for (std::size_t i = 0; i < streams.size(); ++i)
    {
        const auto object_number = std::to_string(4 + i);
        const auto& content = streams[i];

        builder.add_object(object_number + " 0 obj\n<< /Length " + std::to_string(content.size()) +
                           " >>\nstream\n" + content + "\nendstream\nendobj\n");
    }

    return builder.finish();
}
} // namespace

TEST_CASE("page exposes a single /Contents stream", "[content_stream][page]")
{
    auto data = make_page_pdf("4 0 R", {"q 1 0 0 1 0 0 cm"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE_FALSE(contents.is_array());
    REQUIRE(contents.size() == 1);
    REQUIRE(contents.at(0).stream() != nullptr);
}

TEST_CASE("page exposes an array of /Contents streams", "[content_stream][page]")
{
    auto data = make_page_pdf("[4 0 R 5 0 R]", {"q 1 0 0 1 0 0 cm", "Q"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE(contents.is_array());
    REQUIRE(contents.size() == 2);
    REQUIRE_FALSE(contents.empty());

    for (const auto& stream : contents.streams())
    {
        REQUIRE(stream.stream() != nullptr);
    }
}

TEST_CASE("page distinguishes a one-element /Contents array", "[content_stream][page]")
{
    auto data = make_page_pdf("[4 0 R]", {"q 1 0 0 1 0 0 cm"});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE(contents.is_array());
    REQUIRE(contents.size() == 1);
}

TEST_CASE("page without /Contents allocates an empty stream on access", "[content_stream][page]")
{
    auto data = make_page_pdf("", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE_FALSE(contents.empty());
    REQUIRE(contents.size() == 1);
    REQUIRE_FALSE(contents.is_array());
    REQUIRE(contents.at(0).stream() != nullptr);
}

TEST_CASE("page with null /Contents has empty contents", "[content_stream][page]")
{
    auto data = make_page_pdf("null", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE(contents.empty());
    REQUIRE(contents.size() == 0);
    REQUIRE_FALSE(contents.is_array());
}

TEST_CASE("page rejects a /Contents reference that is not a stream", "[content_stream][page]")
{
    auto data = make_page_pdf("3 0 R", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE_THROWS_AS(contents.streams(), logic_exception);
}

TEST_CASE("page rejects an unresolvable /Contents reference", "[content_stream][page]")
{
    auto data = make_page_pdf("99 0 R", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE_THROWS_AS(contents.streams(), parse_exception);
}

TEST_CASE("page rejects a non-reference /Contents array entry", "[content_stream][page]")
{
    auto data = make_page_pdf("[42]", {});
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    auto candidate = doc.catalog().pages().page(0);
    REQUIRE(candidate.has_value());

    auto contents = candidate->content();
    REQUIRE_THROWS_AS(contents.streams(), logic_exception);
}
} // namespace ripper::pdf::core
