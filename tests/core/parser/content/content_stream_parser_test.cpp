#include "ripper/pdf/core/content/content_stream_instructions.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/content/content_stream_parser.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
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

content_stream_instructions parse(std::string_view sv)
{
    return content_stream_parser::parse(b(sv));
}

const content_operation& operation(const content_stream_instructions& instructions,
                                   std::size_t index)
{
    return std::get<content_operation>(instructions[index]);
}

const inline_image& image(const content_stream_instructions& instructions, std::size_t index)
{
    return std::get<inline_image>(instructions[index]);
}
} // namespace

TEST_CASE("content_stream_parser parses operators and operands in order", "[content][parser]")
{
    const auto instructions = parse("q 1 0 0 1 100 200 cm /F1 12 Tf (Hello) Tj Q");

    REQUIRE(instructions.size() == 5);
    REQUIRE(instructions.operation_count() == 5);
    REQUIRE(instructions.inline_image_count() == 0);

    REQUIRE(operation(instructions, 0).operator_name == "q");
    REQUIRE(operation(instructions, 0).operands.empty());

    const auto& cm = operation(instructions, 1);
    REQUIRE(cm.operator_name == "cm");
    REQUIRE(cm.operands.size() == 6);
    REQUIRE(cm.operands[0].is_integer());
    REQUIRE(cm.operands[0].as_number()->as_integer() == 1);
    REQUIRE(cm.operands[4].as_number()->as_integer() == 100);

    const auto& tf = operation(instructions, 2);
    REQUIRE(tf.operator_name == "Tf");
    REQUIRE(tf.operands.size() == 2);
    REQUIRE(tf.operands[0].as_name()->value == "F1");
    REQUIRE(tf.operands[1].as_number()->as_integer() == 12);

    const auto& tj = operation(instructions, 3);
    REQUIRE(tj.operator_name == "Tj");
    REQUIRE(tj.operands.size() == 1);
    REQUIRE(tj.operands[0].is_string());
    REQUIRE(tj.operands[0].as_string()->as_string() == "Hello");

    REQUIRE(operation(instructions, 4).operator_name == "Q");
}

TEST_CASE("content_stream_parser preserves unknown operators", "[content][parser]")
{
    const auto instructions = parse("MyVendorOp Q");

    REQUIRE(instructions.size() == 2);
    REQUIRE(operation(instructions, 0).operator_name == "MyVendorOp");
    REQUIRE(operation(instructions, 0).operands.empty());
    REQUIRE(operation(instructions, 1).operator_name == "Q");
}

TEST_CASE("content_stream_parser treats true, false, and null as operands", "[content][parser]")
{
    const auto instructions = parse("true false null Do");

    REQUIRE(instructions.size() == 1);
    const auto& op = operation(instructions, 0);
    REQUIRE(op.operator_name == "Do");
    REQUIRE(op.operands.size() == 3);
    REQUIRE(op.operands[0].as_boolean()->value);
    REQUIRE_FALSE(op.operands[1].as_boolean()->value);
    REQUIRE(op.operands[2].is_null());
}

TEST_CASE("content_stream_parser parses compound operands", "[content][parser]")
{
    const auto instructions = parse("[/A 1] << /K /V >> Do");

    REQUIRE(instructions.size() == 1);
    const auto& op = operation(instructions, 0);
    REQUIRE(op.operator_name == "Do");
    REQUIRE(op.operands.size() == 2);
    REQUIRE(op.operands[0].is_array());
    REQUIRE(op.operands[0].as_array()->size() == 2);
    REQUIRE(op.operands[1].is_dictionary());
    REQUIRE(op.operands[1].as_dictionary()->get_name("K")->value == "V");
}

TEST_CASE("content_stream_parser keeps n g R as operands and an operator",
          "[content][parser][indirect_reference]")
{
    const auto instructions = parse("1 0 R");

    REQUIRE(instructions.size() == 1);
    const auto& op = operation(instructions, 0);
    REQUIRE(op.operator_name == "R");
    REQUIRE(op.operands.size() == 2);
    REQUIRE(op.operands[0].is_integer());
    REQUIRE(op.operands[1].is_integer());
}

TEST_CASE("content_stream_parser ignores whitespace and comments", "[content][parser]")
{
    REQUIRE(parse("").empty());
    REQUIRE(parse("  \r\n\t ").empty());
    REQUIRE(parse("% a comment\nq % trailing\nQ\n").size() == 2);
}

TEST_CASE("content_stream_parser parses an inline image", "[content][parser][inline_image]")
{
    std::string text = "q BI /W 2 /H 2 /BPC 8 /CS /RGB ID ";
    text.push_back('\x01');
    text.push_back('\x02');
    text += " EI Q";

    const auto instructions = parse(text);

    REQUIRE(instructions.size() == 3);
    REQUIRE(instructions.inline_image_count() == 1);

    const auto& img = image(instructions, 1);
    REQUIRE(img.dictionary.get_number("W")->as_integer() == 2);
    REQUIRE(img.dictionary.get_number("H")->as_integer() == 2);
    REQUIRE(img.dictionary.get_name("CS")->value == "RGB");
    REQUIRE(img.data == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});

    REQUIRE(operation(instructions, 2).operator_name == "Q");
}

TEST_CASE("content_stream_parser honours /L when data contains EI",
          "[content][parser][inline_image]")
{
    const auto instructions = parse("BI /W 1 /H 1 /L 2 ID EI EI\nq");

    REQUIRE(instructions.size() == 2);
    REQUIRE(instructions.inline_image_count() == 1);

    const auto& img = image(instructions, 0);
    REQUIRE(img.data == std::vector<std::byte>{std::byte{'E'}, std::byte{'I'}});
    REQUIRE(operation(instructions, 1).operator_name == "q");
}

TEST_CASE("content_stream_parser rejects a malformed inline image",
          "[content][parser][inline_image]")
{
    REQUIRE_THROWS_AS(parse("BI /W 1 ID abc"), parse_exception);
}

TEST_CASE("content_stream_parser rejects dangling operands", "[content][parser]")
{
    REQUIRE_THROWS_AS(parse("1 2 3"), parse_exception);
}

TEST_CASE("content_stream_parser enforces the parse depth limit", "[content][parser]")
{
    resource_limits limits;
    limits.max_parse_depth = 2;

    REQUIRE_THROWS_AS(content_stream_parser::parse(b("[[[1]]] Do"), limits), parse_exception);
}
} // namespace ripper::pdf::core
