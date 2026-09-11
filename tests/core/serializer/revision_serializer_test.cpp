#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/compressed_cross_reference_table_serializer.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/default_cross_reference_table_serializer.hpp"
#include "ripper/pdf/core/serializer/object/default_object_serializer.hpp"
#include "ripper/pdf/core/serializer/revision_serializer.hpp"
#include "ripper/pdf/core/serializer/trailer/default_trailer_serializer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

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

cross_reference_section make_traditional_section()
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{{0, 65535}, 0, false});
    entries.emplace(1, cross_reference_entry{{1, 0}, 42, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));
    return cross_reference_section{std::move(subsections)};
}

trailer make_trailer_with_root(std::uint32_t root_obj_number)
{
    trailer t{dictionary_object{}};
    t.dictionary().set("Size", object{static_cast<std::int64_t>(2)});
    t.dictionary().set("Root", object{indirect_reference{root_obj_number, 0}});
    return t;
}

revision make_revision(cross_reference_section section, trailer t)
{
    return revision{std::move(section), std::move(t)};
}
} // namespace

TEST_CASE("revision_serializer emits traditional xref block and trailer", "[serializer][revision]")
{
    default_object_serializer obj_ser;
    default_cross_reference_table_serializer xref_ser;
    default_trailer_serializer trailer_ser{obj_ser};

    revision_serializer ser{xref_ser, trailer_ser};

    auto section = make_traditional_section();
    auto t = make_trailer_with_root(1);
    auto rev = make_revision(std::move(section), std::move(t));

    const auto result = ser.serialize(rev, 100);

    const auto s = bytes_to_string(result);

    REQUIRE(s.find("xref\n0 2\n") == 0);
    REQUIRE(s.find("trailer\n") != std::string::npos);
    REQUIRE(s.find("/Root") != std::string::npos);
    REQUIRE(s.find("startxref\n100\n%%EOF\n") != std::string::npos);
}

TEST_CASE("revision_serializer uses custom line break for trailer_tail", "[serializer][revision]")
{
    default_object_serializer obj_ser;
    default_cross_reference_table_serializer xref_ser;
    default_trailer_serializer trailer_ser{obj_ser};
    trailer_ser.set_line_break_character('\r');

    revision_serializer ser{xref_ser, trailer_ser};

    auto section = make_traditional_section();
    auto t = make_trailer_with_root(1);
    auto rev = make_revision(std::move(section), std::move(t));

    const auto result = ser.serialize(rev, 200);

    const auto s = bytes_to_string(result);

    REQUIRE(s.find("startxref\r200\r%%EOF\r") != std::string::npos);
}

TEST_CASE("revision_serializer compressed section emits startxref tail without trailer keyword",
          "[serializer][revision][compressed]")
{
    default_object_serializer obj_ser;
    compressed_cross_reference_table_serializer compressed_ser;
    compressed_ser.set_object_serializer(obj_ser);
    default_trailer_serializer trailer_ser{obj_ser};

    revision_serializer ser{compressed_ser, trailer_ser};

    auto section = make_traditional_section();
    section.set_xref_stream_object_number(7);
    auto t = make_trailer_with_root(1);
    auto rev = make_revision(std::move(section), std::move(t));

    const auto result = ser.serialize(rev, 300);
    const auto s = bytes_to_string(result);

    REQUIRE(s.find("7 0 obj") != std::string::npos);
    REQUIRE(s.find("/Type /XRef") != std::string::npos);
    REQUIRE(s.find("/Root") != std::string::npos);
    REQUIRE(s.find("/Size") != std::string::npos);
    REQUIRE(s.find("trailer\n") == std::string::npos);
    REQUIRE(s.find("startxref\n300\n%%EOF\n") != std::string::npos);
}

} // namespace ripper::pdf::core
