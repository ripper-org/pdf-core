#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/serializer/object/default_object_serializer.hpp"
#include "ripper/pdf/core/serializer/trailer/default_trailer_serializer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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

TEST_CASE("default_trailer_serializer serializes minimal trailer", "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(3)});
    dict.set("Root", object{indirect_reference{1, 0}});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 42);
    const auto s = bytes_to_string(result);

    REQUIRE(s.starts_with("trailer\n"));
    REQUIRE(s.find("/Size 3") != std::string::npos);
    REQUIRE(s.find("/Root 1 0 R") != std::string::npos);
    REQUIRE(s.find("startxref") != std::string::npos);
    REQUIRE(s.find("42") != std::string::npos);
    REQUIRE(s.find("%%EOF") != std::string::npos);
}

TEST_CASE("default_trailer_serializer serializes trailer with all standard keys",
          "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(5)});
    dict.set("Root", object{indirect_reference{10, 0}});
    dict.set("Prev", object{static_cast<std::int64_t>(99)});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 123);
    const auto s = bytes_to_string(result);

    REQUIRE(s.starts_with("trailer\n"));
    REQUIRE(s.find("/Size 5") != std::string::npos);
    REQUIRE(s.find("/Root 10 0 R") != std::string::npos);
    REQUIRE(s.find("/Prev 99") != std::string::npos);
    REQUIRE(s.find("startxref") != std::string::npos);
    REQUIRE(s.find("123") != std::string::npos);
    REQUIRE(s.find("%%EOF") != std::string::npos);
}

TEST_CASE("default_trailer_serializer serializes trailer with ID array", "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(1)});
    dict.set("Root", object{indirect_reference{1, 0}});

    array_object id_arr;
    id_arr.push_back(object{string_object{"original"}});
    id_arr.push_back(object{string_object{"current"}});
    dict.set("ID", object{std::move(id_arr)});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 0);
    const auto s = bytes_to_string(result);

    REQUIRE(s.find("/ID [(original) (current)]") != std::string::npos);
}

TEST_CASE("default_trailer_serializer serializes trailer with unknown keys preserved",
          "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(2)});
    dict.set("Root", object{indirect_reference{1, 0}});
    dict.set("CustomKey", object{name_object{"CustomValue"}});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 7);
    const auto s = bytes_to_string(result);

    REQUIRE(s.find("/CustomKey /CustomValue") != std::string::npos);
}

TEST_CASE("default_trailer_serializer uses custom line break character", "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(3)});
    dict.set("Root", object{indirect_reference{1, 0}});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};
    ser.set_line_break_character('\r');

    const auto result = ser.serialize(t, 42);

    // set_line_break_character does NOT propagate to the embedded object_serializer
    // (unlike default_indirect_object_serializer), so dictionary_object internals use \n.
    const auto s = bytes_to_string(result);
    REQUIRE(s.starts_with("trailer\r<<\n"));
    REQUIRE(s.find("\n/Size 3\n") != std::string::npos);
    REQUIRE(s.find("\n/Root 1 0 R\n") != std::string::npos);
    REQUIRE(s.find(">>\rstartxref\r42\r%%EOF\r") != std::string::npos);
}

TEST_CASE("default_trailer_serializer outputs startxref with zero offset", "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(1)});
    dict.set("Root", object{indirect_reference{1, 0}});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 0);

    REQUIRE(bytes_to_string(result).find("startxref\n0\n") != std::string::npos);
}

TEST_CASE("default_trailer_serializer outputs startxref with large offset", "[serializer][trailer]")
{
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(1)});
    dict.set("Root", object{indirect_reference{1, 0}});
    trailer t{std::move(dict)};

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 999999);

    REQUIRE(bytes_to_string(result).find("startxref\n999999\n") != std::string::npos);
}

TEST_CASE("default_trailer_serializer trailer dictionary_object serialized as-is",
          "[serializer][trailer]")
{
    // Verify the trailer dictionary_object is passed through without any filtering
    dictionary_object dict;
    dict.set("Size", object{static_cast<std::int64_t>(3)});
    dict.set("Root", object{indirect_reference{1, 0}});
    trailer t{std::move(dict)}; // no /Prev or /ID set

    default_object_serializer obj_ser;
    default_trailer_serializer ser{obj_ser};

    const auto result = ser.serialize(t, 42);
    const auto s = bytes_to_string(result);

    // Only /Size and /Root should appear
    REQUIRE(s.find("/Size 3") != std::string::npos);
    REQUIRE(s.find("/Root 1 0 R") != std::string::npos);
    // /Prev should not appear (was not set)
    REQUIRE(s.find("/Prev") == std::string::npos);
}
} // namespace ripper::pdf::core
