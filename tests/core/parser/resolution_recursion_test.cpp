#include "ripper/io/core/reader/memory_reader.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
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

std::vector<std::byte> make_minimal_pdf()
{
    std::string out = "%PDF-1.7\n";

    const auto object1 = out.size();
    out += "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    const auto object2 = out.size();
    out += "2 0 obj\n<< /Type /Pages /Count 0 /Kids [] >>\nendobj\n";

    const auto xref = out.size();
    out += "xref\n0 3\n";
    out += "0000000000 65535 f \n";

    const auto append_entry = [&](std::size_t offset)
    {
        std::ostringstream entry;
        entry << std::setw(10) << std::setfill('0') << offset << " 00000 n \n";
        out += entry.str();
    };

    append_entry(object1);
    append_entry(object2);
    out += "trailer\n<< /Size 3 /Root 1 0 R >>\n";
    out += "startxref\n" + std::to_string(xref) + "\n%%EOF\n";

    return make_bytes(out);
}
} // namespace

TEST_CASE("recursive compressed-object resolution is rejected", "[parser][resolver][recursion]")
{
    auto data = make_minimal_pdf();
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    // Object 5 claims to be compressed inside object stream 5, which requires
    // resolving object 5 while it is already being resolved.
    doc.cross_reference_table().active_section().add_entry(
        cross_reference_entry{indirect_reference{5, 0}, 5U, 0U});

    REQUIRE_THROWS_WITH(doc.resolve_object(indirect_reference{5, 0}),
                        Catch::Matchers::ContainsSubstring("Recursive resolution"));

    // The in-progress guard must be cleaned up after the exception.
    auto* catalog = doc.resolve_object(indirect_reference{1, 0});
    REQUIRE(catalog != nullptr);
    REQUIRE(catalog->content().is_dictionary());
}

TEST_CASE("resolution depth limit is enforced", "[parser][resolver][limits]")
{
    auto data = make_minimal_pdf();
    document doc{std::make_unique<ripper::io::core::memory_reader>(data), nullptr};

    resource_limits limits;
    limits.max_resolution_depth = 0;
    doc.set_limits(limits);

    REQUIRE_THROWS_WITH(doc.resolve_object(indirect_reference{1, 0}),
                        Catch::Matchers::ContainsSubstring("resolution depth"));
}
} // namespace ripper::pdf::core
