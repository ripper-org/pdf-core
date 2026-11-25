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
    doc.set_resource_limits(limits);

    REQUIRE_THROWS_WITH(doc.resolve_object(indirect_reference{1, 0}),
                        Catch::Matchers::ContainsSubstring("resolution depth"));
}

namespace
{
void add_resolved(document& doc, std::uint32_t number, object content)
{
    const auto ref = indirect_reference{number, 0};
    doc.cross_reference_table().active_section().add_entry(cross_reference_entry{
        ref, std::make_unique<indirect_object>(object_identity{&doc, ref}, std::move(content))});
}
} // namespace

TEST_CASE("resolve_object_deep follows reference chains", "[document][resolver][deep]")
{
    document doc{nullptr, nullptr};

    add_resolved(doc, 10, object{indirect_reference{11, 0}});
    add_resolved(doc, 11, object{indirect_reference{12, 0}});
    add_resolved(doc, 12, object{std::int64_t{42}});

    const auto start = object{indirect_reference{10, 0}};
    const auto* value = doc.resolve_object_deep(&start);

    REQUIRE(value != nullptr);
    REQUIRE(value->as_number() != nullptr);
    REQUIRE(value->as_number()->as_integer() == 42);

    // Non-reference values pass through unchanged.
    const auto direct = object{dictionary_object{}};
    REQUIRE(doc.resolve_object_deep(&direct) == &direct);
}

TEST_CASE("resolve_object_deep preserves null objects", "[document][resolver][deep]")
{
    document doc{nullptr, nullptr};

    add_resolved(doc, 20, object{indirect_reference{21, 0}});
    add_resolved(doc, 21, object{});

    const auto chain = object{indirect_reference{20, 0}};
    const auto* chained_null = doc.resolve_object_deep(&chain);
    REQUIRE(chained_null != nullptr);
    REQUIRE(chained_null->is_null());

    const auto null = object{};
    const auto* direct_null = doc.resolve_object_deep(&null);
    REQUIRE(direct_null == &null);

    // Only a null input pointer maps to nullptr.
    REQUIRE(doc.resolve_object_deep(nullptr) == nullptr);

    const auto missing = object{indirect_reference{99, 0}};
    REQUIRE_THROWS_WITH(doc.resolve_object_deep(&missing),
                        Catch::Matchers::ContainsSubstring("not found"));
}

TEST_CASE("resolve_object_deep rejects cyclic chains", "[document][resolver][deep][recursion]")
{
    document doc{nullptr, nullptr};

    add_resolved(doc, 30, object{indirect_reference{31, 0}});
    add_resolved(doc, 31, object{indirect_reference{30, 0}});

    const auto cycle = object{indirect_reference{30, 0}};
    REQUIRE_THROWS_WITH(doc.resolve_object_deep(&cycle),
                        Catch::Matchers::ContainsSubstring("Cyclic indirect reference chain"));
}
} // namespace ripper::pdf::core
