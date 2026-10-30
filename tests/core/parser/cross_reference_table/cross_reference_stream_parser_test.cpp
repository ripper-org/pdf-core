#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_stream_parser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> bytes(std::initializer_list<unsigned int> values)
{
    std::vector<std::byte> out;
    out.reserve(values.size());
    for (const auto value : values)
        out.push_back(static_cast<std::byte>(value));
    return out;
}

array_object widths(std::int64_t w0, std::int64_t w1, std::int64_t w2)
{
    array_object result;
    result.push_back(object{w0});
    result.push_back(object{w1});
    result.push_back(object{w2});
    return result;
}

stream_object make_xref_stream(std::int64_t size, array_object w, std::vector<std::byte> data)
{
    dictionary_object dict;
    dict.set("Type", object{name_object{"XRef"}});
    dict.set("Size", object{size});
    dict.set("W", object{std::move(w)});

    return stream_object{std::move(dict), stream{std::move(data)}};
}
} // namespace

TEST_CASE("cross_reference_stream_parser reads a valid entry", "[parser][xref_stream]")
{
    auto stream_obj = make_xref_stream(1, widths(1, 2, 1), bytes({1, 0x00, 0x0A, 0}));

    const auto section = cross_reference_stream_parser::parse(stream_obj);

    const auto* entry = section.find(0);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->in_use());
    REQUIRE(entry->offset() == 10);
}

TEST_CASE("cross_reference_stream_parser rejects oversized field widths",
          "[parser][xref_stream][bounds]")
{
    // The original 32-bit entry-size sum wrapped to 1 when w1 was 2^32 - 1,
    // allowing the field read to start outside the entry.
    auto stream_obj = make_xref_stream(1, widths(1, 4294967295LL, 1), bytes({1}));
    REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(stream_obj), parse_exception);
}

TEST_CASE("cross_reference_stream_parser rejects unsupported widths",
          "[parser][xref_stream][bounds]")
{
    REQUIRE_THROWS_AS(
        cross_reference_stream_parser::parse(make_xref_stream(1, widths(9, 0, 0), bytes({1}))),
        parse_exception);
    REQUIRE_THROWS_AS(
        cross_reference_stream_parser::parse(make_xref_stream(1, widths(-1, 1, 1), bytes({1}))),
        parse_exception);

    // /W entries must be integers, not reals.
    dictionary_object dict;
    dict.set("Type", object{name_object{"XRef"}});
    dict.set("Size", object{std::int64_t{1}});

    array_object w;
    w.push_back(object{1.5});
    w.push_back(object{std::int64_t{1}});
    w.push_back(object{std::int64_t{1}});
    dict.set("W", object{std::move(w)});

    stream_object stream_obj{std::move(dict), stream{bytes({1})}};
    REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(stream_obj), parse_exception);
}

TEST_CASE("cross_reference_stream_parser requires exactly three widths", "[parser][xref_stream]")
{
    dictionary_object dict;
    dict.set("Type", object{name_object{"XRef"}});
    dict.set("Size", object{std::int64_t{1}});

    array_object w;
    w.push_back(object{std::int64_t{1}});
    w.push_back(object{std::int64_t{1}});
    dict.set("W", object{std::move(w)});

    stream_object stream_obj{std::move(dict), stream{bytes({1, 1})}};
    REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(stream_obj), parse_exception);
}

TEST_CASE("cross_reference_stream_parser rejects zero-width entries", "[parser][xref_stream]")
{
    REQUIRE_THROWS_AS(
        cross_reference_stream_parser::parse(make_xref_stream(1, widths(0, 0, 0), bytes({}))),
        parse_exception);
}

TEST_CASE("cross_reference_stream_parser rejects truncated entries",
          "[parser][xref_stream][bounds]")
{
    // Four bytes are needed for the (1, 2, 1) layout.
    REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(
                          make_xref_stream(1, widths(1, 2, 1), bytes({1, 0x00, 0x0A}))),
                      parse_exception);
}

TEST_CASE("cross_reference_stream_parser validates /Index", "[parser][xref_stream]")
{
    SECTION("odd number of entries")
    {
        auto data = bytes({1, 0x00, 0x0A, 0});
        auto stream_obj = make_xref_stream(1, widths(1, 2, 1), std::move(data));

        array_object index;
        index.push_back(object{std::int64_t{0}});
        stream_obj.dictionary().set("Index", object{std::move(index)});

        REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(stream_obj), parse_exception);
    }

    SECTION("negative first object number")
    {
        auto stream_obj = make_xref_stream(1, widths(1, 2, 1), bytes({1, 0x00, 0x0A, 0}));

        array_object index;
        index.push_back(object{std::int64_t{-1}});
        index.push_back(object{std::int64_t{1}});
        stream_obj.dictionary().set("Index", object{std::move(index)});

        REQUIRE_THROWS_AS(cross_reference_stream_parser::parse(stream_obj), parse_exception);
    }
}

TEST_CASE("cross_reference_stream_parser treats unknown entry types as free",
          "[parser][xref_stream]")
{
    auto stream_obj = make_xref_stream(1, widths(1, 2, 1), bytes({7, 0xFF, 0xFF, 0xFF}));

    const auto section = cross_reference_stream_parser::parse(stream_obj);

    const auto* entry = section.find(0);
    REQUIRE(entry != nullptr);
    REQUIRE_FALSE(entry->in_use());
}
} // namespace ripper::pdf::core
