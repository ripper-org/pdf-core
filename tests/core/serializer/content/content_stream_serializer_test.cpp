#include "ripper/pdf/core/document/catalog/pages/page/content/instruction_stream.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/serializer/content/content_stream_serializer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<content_item> collect(const instruction_stream& stream)
{
    std::vector<content_item> items;
    for (const auto& item : stream.operations())
        items.push_back(item);
    return items;
}

std::vector<content_item> parse(std::span<const std::byte> bytes)
{
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
} // namespace

TEST_CASE("content_stream_serializer round-trips instructions", "[content][serializer]")
{
    const std::vector<content_item> original{
        content_operation{"q", {}},
        content_operation{
            "cm",
            {object{1.0}, object{0.0}, object{0.0}, object{1.0}, object{100.0}, object{200.0}}},
        content_operation{"Tf", {object{name_object{"F1"}}, object{std::int64_t{12}}}},
        content_operation{"Tj", {object{string_object{"Hello (world)"}}}},
        content_operation{"Q", {}},
    };

    const auto bytes =
        content_stream_serializer::serialize(std::span<const content_item>{original});
    const auto reparsed = parse(bytes);

    REQUIRE(reparsed.size() == original.size());
    REQUIRE(operation(reparsed, 0).operator_name == "q");

    const auto& cm = operation(reparsed, 1);
    REQUIRE(cm.operator_name == "cm");
    REQUIRE(cm.operands.size() == 6);
    REQUIRE(cm.operands[4].as_number()->as_real() == 100.0);

    const auto& tf = operation(reparsed, 2);
    REQUIRE(tf.operands[0].as_name()->value == "F1");
    REQUIRE(tf.operands[1].as_number()->as_integer() == 12);

    REQUIRE(operation(reparsed, 3).operands[0].as_string()->as_string() == "Hello (world)");
    REQUIRE(operation(reparsed, 4).operator_name == "Q");
}

TEST_CASE("content_stream_serializer preserves booleans and null", "[content][serializer]")
{
    const content_item original{content_operation{
        "X", {object{true}, object{false}, object{}, object{name_object{"A B"}}}}};

    const auto bytes = content_stream_serializer::serialize(original);
    const auto reparsed = parse(bytes);

    const auto& op = operation(reparsed, 0);
    REQUIRE(op.operands.size() == 4);
    REQUIRE(op.operands[0].as_boolean()->value);
    REQUIRE_FALSE(op.operands[1].as_boolean()->value);
    REQUIRE(op.operands[2].is_null());
    REQUIRE(op.operands[3].as_name()->value == "A B");
}

TEST_CASE("content_stream_serializer round-trips inline image data",
          "[content][serializer][inline_image]")
{
    // Bytes chosen to exercise leading/trailing white space and EI-looking data.
    const std::vector<std::byte> data{std::byte{' '}, std::byte{'E'}, std::byte{'I'},
                                      std::byte{0x00}, std::byte{'\n'}};

    dictionary_object dictionary;
    dictionary.set("W", object{std::int64_t{4}});
    dictionary.set("H", object{std::int64_t{1}});
    dictionary.set("BPC", object{std::int64_t{8}});
    dictionary.set("CS", object{name_object{"DeviceGray"}});
    dictionary.set("L", object{static_cast<std::int64_t>(data.size())});

    const std::vector<content_item> original{
        content_operation{"q", {}},
        inline_image{dictionary, data},
        content_operation{"Q", {}},
    };

    const auto bytes =
        content_stream_serializer::serialize(std::span<const content_item>{original});
    const auto reparsed = parse(bytes);

    REQUIRE(reparsed.size() == 3);
    REQUIRE(image(reparsed, 1).data == data);
    REQUIRE(operation(reparsed, 2).operator_name == "Q");
}

TEST_CASE("content_stream_serializer serializes a live instruction_stream", "[content][serializer]")
{
    const std::vector<std::byte> bytes{static_cast<std::byte>('q'), static_cast<std::byte>(' '),
                                       static_cast<std::byte>('Q')};
    const instruction_stream stream{bytes};

    const auto serialized = content_stream_serializer::serialize(stream);

    REQUIRE(serialized ==
            content_stream_serializer::serialize(std::span<const content_item>{collect(stream)}));
}

TEST_CASE("content_stream_serializer emits nothing for an empty stream", "[content][serializer]")
{
    const std::span<const content_item> empty_items{};
    REQUIRE(content_stream_serializer::serialize(empty_items).empty());

    const instruction_stream empty_stream{std::span<const std::byte>{}};
    REQUIRE(content_stream_serializer::serialize(empty_stream).empty());
}
} // namespace ripper::pdf::core
