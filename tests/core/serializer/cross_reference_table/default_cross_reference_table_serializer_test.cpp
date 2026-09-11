#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/helpers/object_identity.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/default_cross_reference_table_serializer.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ripper::pdf::core
{
namespace
{
std::string bytes_to_string(const std::vector<std::byte>& bytes)
{
    std::string result;
    result.reserve(bytes.size());
    for (auto b : bytes)
        result += static_cast<char>(b);
    return result;
}
} // namespace

TEST_CASE("default_cross_reference_table_serializer serializes single subsection",
          "[serializer][xref]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{{0, 65535}, 0, false});
    entries.emplace(1, cross_reference_entry{{1, 0}, 42, true});
    entries.emplace(2, cross_reference_entry{{2, 0}, 100, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    const auto expected =
        "xref\n0 3\n0000000000 65535 f\r\n0000000042 00000 n\r\n0000000100 00000 n\r\n";
    REQUIRE(bytes_to_string(result) == expected);
}

TEST_CASE("default_cross_reference_table_serializer serializes single in-use entry",
          "[serializer][xref]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(5, cross_reference_entry{{5, 0}, 12345, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(5, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    REQUIRE(bytes_to_string(result) == "xref\n5 1\n0000012345 00000 n\r\n");
}

TEST_CASE("default_cross_reference_table_serializer serializes free entry only",
          "[serializer][xref]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{{0, 65535}, 0, false});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    REQUIRE(bytes_to_string(result) == "xref\n0 1\n0000000000 65535 f\r\n");
}

TEST_CASE("default_cross_reference_table_serializer serializes multiple subsections",
          "[serializer][xref]")
{
    cross_reference_subsection::entry_map entries0;
    entries0.emplace(0, cross_reference_entry{{0, 65535}, 0, false});
    entries0.emplace(1, cross_reference_entry{{1, 0}, 100, true});

    cross_reference_subsection::entry_map entries5;
    entries5.emplace(5, cross_reference_entry{{5, 0}, 200, true});
    entries5.emplace(6, cross_reference_entry{{6, 0}, 300, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries0));
    subsections.emplace_back(5, std::move(entries5));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    const auto expected = "xref\n0 2\n0000000000 65535 f\r\n0000000100 00000 n\r\n"
                          "5 2\n0000000200 00000 n\r\n0000000300 00000 n\r\n";
    REQUIRE(bytes_to_string(result) == expected);
}

TEST_CASE("default_cross_reference_table_serializer serializes empty section", "[serializer][xref]")
{
    cross_reference_section section{{}};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    REQUIRE(bytes_to_string(result) == "xref\n");
}

TEST_CASE("default_cross_reference_table_serializer serializes entry with missing offset as zero",
          "[serializer][xref]")
{
    // object_identity requires a non-null document pointer.
    auto doc = document{nullptr, nullptr};

    // In-memory entry with a resolved object but no file offset — offset is nullopt.
    auto obj = std::make_unique<indirect_object>(object_identity{&doc, {1, 0}},
                                                 object{static_cast<std::int64_t>(42)});

    cross_reference_subsection::entry_map entries;
    entries.emplace(1, cross_reference_entry{{1, 0}, std::move(obj)});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(1, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result = ser.serialize(section, trailer{dictionary_object{}});

    // offset.value_or(0) → offset is nullopt, so 0; entry is in-use with gen 0
    REQUIRE(bytes_to_string(result) == "xref\n1 1\n0000000000 00000 n\r\n");
}

TEST_CASE("default_cross_reference_table_serializer formats 20-byte entries correctly",
          "[serializer][xref]")
{
    // Verify each entry is exactly 20 bytes per PDF spec §7.5.4
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{{0, 65535}, 0, false});
    entries.emplace(1, cross_reference_entry{{1, 0}, 9999999999, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    default_cross_reference_table_serializer ser;
    const auto result_bytes = ser.serialize(section, trailer{dictionary_object{}});

    // The output has: "xref\n" + "0 2\n" + entry0 + entry1
    // Entry format: 10-digit offset + ' ' + 5-digit generation + ' ' + n/f + "\r\n" = 20
    // We extract just the entry portions to verify 20-byte format
    const auto s = bytes_to_string(result_bytes);

    // Find the two entry lines
    const auto entry0_start = s.find("0000000000");
    REQUIRE(entry0_start != std::string::npos);

    // Entry ends at "\r\n" and next character starts
    const auto entry0_end = entry0_start + 20;
    REQUIRE(s.substr(entry0_start, 10) == "0000000000");
    REQUIRE(s[entry0_start + 10] == ' ');
    REQUIRE(s.substr(entry0_start + 11, 5) == "65535");
    REQUIRE(s[entry0_start + 16] == ' ');
    REQUIRE(s[entry0_start + 17] == 'f');
    REQUIRE(s[entry0_start + 18] == '\r');
    REQUIRE(s[entry0_start + 19] == '\n');
}
} // namespace ripper::pdf::core
