#include "ripper/pdf/core/document/catalog/pages/page/content/instruction_stream.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/filter/filter_manager.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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

std::vector<content_item> collect(const instruction_stream& stream)
{
    std::vector<content_item> items;
    for (const auto& item : stream.operations())
        items.push_back(item);
    return items;
}

std::vector<content_item> parse(std::string_view sv)
{
    const auto bytes = b(sv);
    return collect(instruction_stream{bytes});
}

const content_operation& operation(const std::vector<content_item>& items, std::size_t index)
{
    return std::get<content_operation>(items[index]);
}

const inline_image& image(const std::vector<content_item>& items, std::size_t index)
{
    return std::get<inline_image>(items[index]);
}

std::vector<std::byte> copy_of(std::span<const std::byte> data)
{
    return {data.begin(), data.end()};
}
} // namespace

TEST_CASE("instruction_stream parses operators and operands in order",
          "[content][instruction_stream]")
{
    const auto items = parse("q 1 0 0 1 100 200 cm /F1 12 Tf (Hello) Tj Q");

    REQUIRE(items.size() == 5);

    REQUIRE(operation(items, 0).operator_name == "q");
    REQUIRE(operation(items, 0).operands.empty());

    const auto& cm = operation(items, 1);
    REQUIRE(cm.operator_name == "cm");
    REQUIRE(cm.operands.size() == 6);
    REQUIRE(cm.operands[0].is_integer());
    REQUIRE(cm.operands[0].as_number()->as_integer() == 1);
    REQUIRE(cm.operands[4].as_number()->as_integer() == 100);

    const auto& tf = operation(items, 2);
    REQUIRE(tf.operator_name == "Tf");
    REQUIRE(tf.operands.size() == 2);
    REQUIRE(tf.operands[0].as_name()->value == "F1");
    REQUIRE(tf.operands[1].as_number()->as_integer() == 12);

    const auto& tj = operation(items, 3);
    REQUIRE(tj.operator_name == "Tj");
    REQUIRE(tj.operands.size() == 1);
    REQUIRE(tj.operands[0].is_string());
    REQUIRE(tj.operands[0].as_string()->as_string() == "Hello");

    REQUIRE(operation(items, 4).operator_name == "Q");
}

TEST_CASE("instruction_stream preserves unknown operators", "[content][instruction_stream]")
{
    const auto items = parse("MyVendorOp Q");

    REQUIRE(items.size() == 2);
    REQUIRE(operation(items, 0).operator_name == "MyVendorOp");
    REQUIRE(operation(items, 0).operands.empty());
    REQUIRE(operation(items, 1).operator_name == "Q");
}

TEST_CASE("instruction_stream treats true, false, and null as operands",
          "[content][instruction_stream]")
{
    const auto items = parse("true false null Do");

    REQUIRE(items.size() == 1);
    const auto& op = operation(items, 0);
    REQUIRE(op.operator_name == "Do");
    REQUIRE(op.operands.size() == 3);
    REQUIRE(op.operands[0].as_boolean()->value);
    REQUIRE_FALSE(op.operands[1].as_boolean()->value);
    REQUIRE(op.operands[2].is_null());
}

TEST_CASE("instruction_stream parses compound operands", "[content][instruction_stream]")
{
    const auto items = parse("[/A 1] << /K /V >> Do");

    REQUIRE(items.size() == 1);
    const auto& op = operation(items, 0);
    REQUIRE(op.operator_name == "Do");
    REQUIRE(op.operands.size() == 2);
    REQUIRE(op.operands[0].is_array());
    REQUIRE(op.operands[0].as_array()->size() == 2);
    REQUIRE(op.operands[1].is_dictionary());
    REQUIRE(op.operands[1].as_dictionary()->get_name("K")->value == "V");
}

TEST_CASE("instruction_stream keeps n g R as operands and an operator",
          "[content][instruction_stream][indirect_reference]")
{
    const auto items = parse("1 0 R");

    REQUIRE(items.size() == 1);
    const auto& op = operation(items, 0);
    REQUIRE(op.operator_name == "R");
    REQUIRE(op.operands.size() == 2);
    REQUIRE(op.operands[0].is_integer());
    REQUIRE(op.operands[1].is_integer());
}

TEST_CASE("instruction_stream ignores whitespace and comments", "[content][instruction_stream]")
{
    REQUIRE(parse("").empty());
    REQUIRE(parse("  \r\n\t ").empty());
    REQUIRE(parse("% a comment\nq % trailing\nQ\n").size() == 2);
}

TEST_CASE("instruction_stream parses an inline image",
          "[content][instruction_stream][inline_image]")
{
    std::string text = "q BI /W 2 /H 2 /BPC 8 /CS /RGB ID ";
    text.push_back('\x01');
    text.push_back('\x02');
    text += " EI Q";

    const auto items = parse(text);

    REQUIRE(items.size() == 3);

    const auto& img = image(items, 1);
    REQUIRE(img.dictionary.get_number("W")->as_integer() == 2);
    REQUIRE(img.dictionary.get_number("H")->as_integer() == 2);
    REQUIRE(img.dictionary.get_name("CS")->value == "RGB");
    REQUIRE(img.data == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});

    REQUIRE(operation(items, 2).operator_name == "Q");
}

TEST_CASE("instruction_stream honours /L when data contains EI",
          "[content][instruction_stream][inline_image]")
{
    const auto items = parse("BI /W 1 /H 1 /L 2 ID EI EI\nq");

    REQUIRE(items.size() == 2);

    const auto& img = image(items, 0);
    REQUIRE(img.data == std::vector<std::byte>{std::byte{'E'}, std::byte{'I'}});
    REQUIRE(operation(items, 1).operator_name == "q");
}

TEST_CASE("instruction_stream rejects a malformed inline image",
          "[content][instruction_stream][inline_image]")
{
    REQUIRE_THROWS_AS(parse("BI /W 1 ID abc"), parse_exception);
}

TEST_CASE("instruction_stream rejects dangling operands", "[content][instruction_stream]")
{
    REQUIRE_THROWS_AS(parse("1 2 3"), parse_exception);
}

TEST_CASE("instruction_stream enforces the parse depth limit", "[content][instruction_stream]")
{
    resource_limits limits;
    limits.max_parse_depth = 2;

    const auto bytes = b("[[[1]]] Do");
    REQUIRE_THROWS_AS(collect(instruction_stream{bytes, limits}), parse_exception);
}

TEST_CASE("instruction_stream tokens yields the raw token sequence",
          "[content][instruction_stream][tokens]")
{
    const auto bytes = b("q 1 0 cm");
    const instruction_stream stream{bytes};

    std::vector<std::string> operators;
    std::size_t operand_count = 0;

    for (const auto& token : stream.tokens())
    {
        if (token.kind == content_stream_token_kind::operator_name)
            operators.push_back(token.operator_name);
        else if (token.kind == content_stream_token_kind::operand)
            ++operand_count;
    }

    REQUIRE(operators == std::vector<std::string>{"q", "cm"});
    REQUIRE(operand_count == 2);
}

TEST_CASE("instruction_stream over an empty byte range has no items",
          "[content][instruction_stream]")
{
    const instruction_stream stream{std::span<const std::byte>{}};

    REQUIRE(stream.empty());
    REQUIRE(stream.operations().begin() == stream.operations().end());
    REQUIRE(collect(stream).empty());
}

TEST_CASE("instruction_stream concatenates several content streams",
          "[content][instruction_stream][array]")
{
    dictionary_object dict;

    stream_object first{dict, stream{b("q 1 0 0 1 0 0 cm")}};
    stream_object second{dict, stream{b("Q")}};

    std::vector<stream_object*> parts{&first, &second};
    const instruction_stream stream{parts};

    const auto items = collect(stream);

    REQUIRE(items.size() == 3);
    REQUIRE(operation(items, 0).operator_name == "q");
    REQUIRE(operation(items, 1).operator_name == "cm");
    REQUIRE(operation(items, 2).operator_name == "Q");
}

TEST_CASE("instruction_stream decodes a filtered stream without touching raw bytes",
          "[content][instruction_stream][raw]")
{
    dictionary_object dict;
    dict.set("Filter", object{name_object{"FlateDecode"}});

    const auto plain = b("q 1 0 0 1 0 0 cm Q");
    const auto encoded = filter_manager::encode(dict, plain);

    stream_object object{dict, stream{encoded}};

    REQUIRE(copy_of(object.raw()) == encoded);

    const instruction_stream stream{object};
    const auto items = collect(stream);

    REQUIRE(items.size() == 3);
    REQUIRE(operation(items, 0).operator_name == "q");
    REQUIRE(operation(items, 1).operator_name == "cm");
    REQUIRE(operation(items, 2).operator_name == "Q");

    // Decoding never touches the stored payload.
    REQUIRE(copy_of(object.raw()) == encoded);
}

TEST_CASE("stream_object decode borrows plain bytes and owns filtered bytes",
          "[stream_object][decode]")
{
    const auto plain = b("q Q");

    const stream_object unfiltered{dictionary_object{}, stream{plain}};
    const auto borrowed = unfiltered.decode();

    REQUIRE_FALSE(borrowed.owns());
    REQUIRE(copy_of(borrowed.bytes()) == plain);

    dictionary_object dict;
    dict.set("Filter", object{name_object{"FlateDecode"}});

    const auto encoded = filter_manager::encode(dict, plain);
    const stream_object filtered{dict, stream{encoded}};

    auto decoded = filtered.decode();
    REQUIRE(decoded.owns());
    REQUIRE(copy_of(decoded.bytes()) == plain);
    REQUIRE(copy_of(filtered.raw()) == encoded);

    auto released = std::move(decoded).release();
    REQUIRE(released.has_value());
    REQUIRE(*released == plain);
}

TEST_CASE("instruction_stream owns bytes when it concatenates several streams",
          "[content][instruction_stream][copy]")
{
    const auto make_merged = []()
    {
        dictionary_object dict;
        stream_object first{dict, stream{b("q")}};
        stream_object second{dict, stream{b("Q")}};

        std::vector<stream_object*> parts{&first, &second};
        return instruction_stream{parts};
    };

    // The source stream objects are gone, but the merged bytes are owned.
    const auto stream = make_merged();
    const auto items = collect(stream);

    REQUIRE(items.size() == 2);
    REQUIRE(operation(items, 0).operator_name == "q");
    REQUIRE(operation(items, 1).operator_name == "Q");
}
} // namespace ripper::pdf::core
