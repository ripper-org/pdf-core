#include "ripper/pdf/core/content/content_stream_instructions.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/lexer/content_stream_lexer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> b(std::string_view sv)
{
    std::vector<std::byte> v;
    v.reserve(sv.size());
    for (const char c : sv)
        v.push_back(static_cast<std::byte>(c));
    return v;
}
} // namespace

TEST_CASE("content_stream_lexer emits operators and operands in order", "[content][lexer]")
{
    const auto bytes = b("q 1 0 0 1 100 200 cm /F1");
    content_stream_lexer lexer{bytes};

    const auto q = lexer.next();
    REQUIRE(q.kind == content_stream_token_kind::operator_name);
    REQUIRE(q.operator_name == "q");

    for (int expected : {1, 0, 0, 1, 100, 200})
    {
        const auto token = lexer.next();
        REQUIRE(token.kind == content_stream_token_kind::operand);
        REQUIRE(token.operand.is_integer());
        REQUIRE(token.operand.as_number()->as_integer() == expected);
    }

    const auto cm = lexer.next();
    REQUIRE(cm.kind == content_stream_token_kind::operator_name);
    REQUIRE(cm.operator_name == "cm");

    const auto name = lexer.next();
    REQUIRE(name.kind == content_stream_token_kind::operand);
    REQUIRE(name.operand.as_name()->value == "F1");

    REQUIRE(lexer.next().kind == content_stream_token_kind::end_of_stream);
}

TEST_CASE("content_stream_lexer treats true, false, and null as operands", "[content][lexer]")
{
    const auto bytes = b("true false null Do");
    content_stream_lexer lexer{bytes};

    const auto t = lexer.next();
    REQUIRE(t.kind == content_stream_token_kind::operand);
    REQUIRE(t.operand.as_boolean()->value);

    const auto f = lexer.next();
    REQUIRE(f.kind == content_stream_token_kind::operand);
    REQUIRE_FALSE(f.operand.as_boolean()->value);

    const auto n = lexer.next();
    REQUIRE(n.kind == content_stream_token_kind::operand);
    REQUIRE(n.operand.is_null());

    REQUIRE(lexer.next().operator_name == "Do");
}

TEST_CASE("content_stream_lexer does not collapse n g R into a reference",
          "[content][lexer][indirect_reference]")
{
    const auto bytes = b("1 0 R");
    content_stream_lexer lexer{bytes};

    const auto first = lexer.next();
    REQUIRE(first.kind == content_stream_token_kind::operand);
    REQUIRE(first.operand.is_integer());

    const auto second = lexer.next();
    REQUIRE(second.kind == content_stream_token_kind::operand);
    REQUIRE(second.operand.is_integer());

    // `R` is just an operator name in content streams, not a reference marker.
    const auto third = lexer.next();
    REQUIRE(third.kind == content_stream_token_kind::operator_name);
    REQUIRE(third.operator_name == "R");
}

TEST_CASE("content_stream_lexer emits an inline image token", "[content][lexer][inline_image]")
{
    std::string text = "BI /W 2 /H 2 /BPC 8 /CS /RGB ID ";
    text.push_back('\x01');
    text.push_back('\x02');
    text += " EI";

    const auto bytes = b(text);
    content_stream_lexer lexer{bytes};

    const auto token = lexer.next();
    REQUIRE(token.kind == content_stream_token_kind::inline_image);
    REQUIRE(token.image.dictionary.get_number("W")->as_integer() == 2);
    REQUIRE(token.image.dictionary.get_name("CS")->value == "RGB");
    REQUIRE(token.image.data == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});

    REQUIRE(lexer.next().kind == content_stream_token_kind::end_of_stream);
}

TEST_CASE("content_stream_lexer rejects a malformed inline image", "[content][lexer][inline_image]")
{
    const auto bytes = b("BI /W 1 ID abc");
    content_stream_lexer lexer{bytes};
    REQUIRE_THROWS_AS(lexer.next(), parse_exception);
}
} // namespace ripper::pdf::core
