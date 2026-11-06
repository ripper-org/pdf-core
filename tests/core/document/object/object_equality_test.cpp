#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/util/byte.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
static_assert(noexcept(std::declval<const object&>() == std::declval<const object&>()),
              "object equality must not throw: it is declared noexcept");
static_assert(noexcept(std::declval<const number_object&>() ==
                       std::declval<const number_object&>()));
static_assert(noexcept(std::declval<const name_object&>() == std::declval<const name_object&>()));
static_assert(noexcept(std::declval<const array_object&>() == std::declval<const array_object&>()));
static_assert(noexcept(std::declval<const dictionary_object&>() ==
                       std::declval<const dictionary_object&>()));
static_assert(noexcept(std::declval<const stream_object&>() ==
                       std::declval<const stream_object&>()));

stream_object make_stream(const std::string& text, const std::string& filter = {})
{
    dictionary_object dict;

    if (!filter.empty())
        dict.set("Filter", object{name_object{filter}});

    stream_object result{dict, stream{byte::to_bytes(text)}};
    result.sync_length();
    return result;
}
} // namespace

TEST_CASE("object equality distinguishes alternatives", "[object][equality]")
{
    REQUIRE(object{} == object{});
    REQUIRE(object{bool{true}} == object{bool{true}});
    REQUIRE_FALSE(object{bool{true}} == object{bool{false}});
    REQUIRE_FALSE(object{std::int64_t{1}} == object{std::int64_t{2}});
    REQUIRE(object{std::int64_t{1}} == object{std::int64_t{1}});
    REQUIRE(object{1.5} == object{1.5});
    REQUIRE_FALSE(object{1.5} == object{2.5});
    REQUIRE(object{name_object{"Type"}} == object{name_object{"Type"}});
    REQUIRE_FALSE(object{name_object{"Type"}} == object{name_object{"Pages"}});
    REQUIRE(object{indirect_reference{3, 1}} == object{indirect_reference{3, 1}});
    REQUIRE_FALSE(object{indirect_reference{3, 1}} == object{indirect_reference{3, 0}});

    // Different alternatives are never equal, even with similar payloads.
    REQUIRE_FALSE(object{name_object{"1"}} == object{std::string{"1"}});
    REQUIRE_FALSE(object{std::int64_t{1}} == object{1.0});
    REQUIRE_FALSE(object{} == object{bool{false}});
}

TEST_CASE("string equality compares decoded bytes only", "[object][equality][string]")
{
    const string_object literal{"abc", byte::to_bytes("abc"), string_object::encoding::identity,
                                string_object::form::literal};
    const string_object hex{"abc", byte::to_bytes("616263"), string_object::encoding::identity,
                            string_object::form::hex};

    REQUIRE(object{literal} == object{hex});
    REQUIRE(object{string_object{"abc"}} == object{literal});
    REQUIRE_FALSE(object{string_object{"abc"}} == object{string_object{"abd"}});
    REQUIRE_FALSE(object{string_object{"abc"}} == object{string_object{"abcd"}});
}

TEST_CASE("array equality is element-wise", "[object][equality][array]")
{
    array_object lhs;
    lhs.push_back(object{std::int64_t{1}});
    lhs.push_back(object{name_object{"A"}});

    array_object rhs;
    rhs.push_back(object{std::int64_t{1}});
    rhs.push_back(object{name_object{"A"}});

    array_object shorter;
    shorter.push_back(object{std::int64_t{1}});

    array_object different;
    different.push_back(object{std::int64_t{1}});
    different.push_back(object{name_object{"B"}});

    REQUIRE(object{lhs} == object{rhs});
    REQUIRE_FALSE(object{lhs} == object{shorter});
    REQUIRE_FALSE(object{lhs} == object{different});

    array_object empty;
    REQUIRE(object{empty} == object{array_object{}});
}

TEST_CASE("dictionary equality ignores key order", "[object][equality][dictionary]")
{
    dictionary_object lhs;
    lhs.set("Type", object{name_object{"Page"}});
    lhs.set("Count", object{std::int64_t{2}});

    dictionary_object rhs;
    rhs.set("Count", object{std::int64_t{2}});
    rhs.set("Type", object{name_object{"Page"}});

    dictionary_object extra = rhs;
    extra.set("Extra", object{bool{true}});

    dictionary_object changed = rhs;
    changed.set("Count", object{std::int64_t{3}});

    REQUIRE(object{lhs} == object{rhs});
    REQUIRE_FALSE(object{lhs} == object{extra});
    REQUIRE_FALSE(object{lhs} == object{changed});
    REQUIRE(object{dictionary_object{}} == object{dictionary_object{}});
}

TEST_CASE("nested object equality recurses", "[object][equality][nested]")
{
    dictionary_object inner_lhs;
    inner_lhs.set("Key", object{std::string{"value"}});

    dictionary_object inner_rhs;
    inner_rhs.set("Key", object{std::string{"other"}});

    array_object lhs_array;
    lhs_array.push_back(object{inner_lhs});

    array_object rhs_array;
    rhs_array.push_back(object{inner_rhs});

    REQUIRE_FALSE(object{lhs_array} == object{rhs_array});
    REQUIRE(object{lhs_array} == object{lhs_array});
}

TEST_CASE("stream equality compares dictionary and payload", "[object][equality][stream]")
{
    REQUIRE(object{make_stream("payload")} == object{make_stream("payload")});
    REQUIRE_FALSE(object{make_stream("payload")} == object{make_stream("playloa")});
    REQUIRE_FALSE(object{make_stream("payload")} == object{make_stream("payload", "FlateDecode")});

    // A stream and a dictionary with the same entries are distinct objects.
    const stream_object stream_value = make_stream("payload");
    REQUIRE_FALSE(object{stream_value} == object{stream_value.dictionary()});
}

TEST_CASE("moved-from objects compare by their empty alternatives", "[object][equality]")
{
    object moved_from_dictionary{dictionary_object{}};
    const object dictionary_holder{std::move(moved_from_dictionary)};

    object other_moved_from_dictionary{dictionary_object{}};
    const object other_dictionary_holder{std::move(other_moved_from_dictionary)};

    REQUIRE(moved_from_dictionary == other_moved_from_dictionary);
    REQUIRE_FALSE(moved_from_dictionary == object{dictionary_object{}});

    object moved_from_stream{make_stream("payload")};
    const object stream_holder{std::move(moved_from_stream)};

    object other_moved_from_stream{make_stream("payload")};
    const object other_stream_holder{std::move(other_moved_from_stream)};

    REQUIRE(moved_from_stream == other_moved_from_stream);
    REQUIRE_FALSE(moved_from_stream == object{make_stream("payload")});
    REQUIRE_FALSE(moved_from_stream == moved_from_dictionary);
}

TEST_CASE("object equality reports inequality through !=", "[object][equality]")
{
    REQUIRE(object{std::int64_t{1}} != object{std::int64_t{2}});
    REQUIRE_FALSE(object{std::int64_t{1}} != object{std::int64_t{1}});
}

} // namespace ripper::pdf::core
