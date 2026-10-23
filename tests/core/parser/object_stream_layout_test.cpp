#include "core/parser/object_stream_layout.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> bytes(std::string_view text)
{
    std::vector<std::byte> out;
    out.reserve(text.size());
    for (const char c : text)
        out.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    return out;
}
} // namespace

// The layout is a validated result object, not a mutable aggregate.
static_assert(!std::is_default_constructible_v<object_stream_layout>);
static_assert(!std::is_default_constructible_v<object_stream_layout::entry>);
static_assert(!std::is_default_constructible_v<object_stream_layout::entry::layout_key>);
static_assert(std::is_copy_constructible_v<object_stream_layout::entry>);

TEST_CASE("object_stream_layout exposes validated borrowed bodies", "[parser][objstm][layout]")
{
    document doc{nullptr, nullptr};

    dictionary_object dict;
    dict.set("N", object{static_cast<std::int64_t>(2)});
    dict.set("First", object{static_cast<std::int64_t>(8)});

    // Header: two (object number, relative offset) pairs.
    // Body:  "AAAAAAAB", with the second object starting at relative offset 7.
    const auto content = bytes("1 0\n2 7\nAAAAAAAB");

    const auto layout = object_stream_layout::parse(content, dict, doc.limits());
    const auto entries = layout.entries();

    REQUIRE(layout.count() == 2);
    REQUIRE(layout.first().value() == 8);
    REQUIRE(entries.size() == 2);

    REQUIRE(entries[0].object_number() == 1);
    REQUIRE(entries[0].offset().value() == 8);
    REQUIRE(entries[0].length().value() == 7);
    REQUIRE(entries[0].body().data() == content.data() + 8);
    REQUIRE(entries[0].body().size() == 7);

    REQUIRE(entries[1].object_number() == 2);
    REQUIRE(entries[1].offset().value() == 15);
    REQUIRE(entries[1].length().value() == 1);
    REQUIRE(entries[1].body().data() == content.data() + 15);
}

TEST_CASE("object_stream_layout cannot be constructed from invalid content",
          "[parser][objstm][layout]")
{
    document doc{nullptr, nullptr};

    dictionary_object dict;
    dict.set("N", object{static_cast<std::int64_t>(2)});
    dict.set("First", object{static_cast<std::int64_t>(8)});

    const auto truncated = bytes("1 0\n");

    REQUIRE_THROWS_AS(object_stream_layout::parse(truncated, dict, doc.limits()), parse_exception);
}
} // namespace ripper::pdf::core
