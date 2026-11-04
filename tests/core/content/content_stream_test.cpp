#include "ripper/pdf/core/content/content_stream.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] content_operation make_operation(std::string operator_name,
                                               std::vector<object> operands = {})
{
    return content_operation{std::move(operator_name), std::move(operands)};
}

[[nodiscard]] content_operation make_transform(double a, double b, double c, double d, double e,
                                               double f)
{
    return content_operation{"cm",
                             {object{a}, object{b}, object{c}, object{d}, object{e}, object{f}}};
}

[[nodiscard]] content_operation& as_operation(content_item& item)
{
    return std::get<content_operation>(item);
}

[[nodiscard]] const content_operation& as_operation(const content_item& item)
{
    return std::get<content_operation>(item);
}

[[nodiscard]] const inline_image& as_image(const content_item& item)
{
    return std::get<inline_image>(item);
}

[[nodiscard]] inline_image make_image(std::vector<std::byte> data)
{
    dictionary_object dictionary;
    dictionary.set("W", object{std::int64_t{2}});
    dictionary.set("H", object{std::int64_t{2}});
    dictionary.set("BPC", object{std::int64_t{8}});
    dictionary.set("CS", object{name_object{"DeviceRGB"}});
    return inline_image{std::move(dictionary), std::move(data)};
}
} // namespace

TEST_CASE("a new content_stream is empty", "[content][content_stream]")
{
    const content_stream stream;

    REQUIRE(stream.empty());
    REQUIRE(stream.size() == 0);
    REQUIRE(stream.operation_count() == 0);
    REQUIRE(stream.inline_image_count() == 0);
    REQUIRE(stream.items().empty());
}

TEST_CASE("content_stream append preserves operation order and operands",
          "[content][content_stream]")
{
    content_stream stream;

    stream.append(make_operation("q"));
    stream.append(make_transform(1.0, 0.0, 0.0, 1.0, 100.0, 200.0));
    stream.append(make_operation("Q"));

    REQUIRE(stream.size() == 3);
    REQUIRE_FALSE(stream.empty());
    REQUIRE(stream.operation_count() == 3);

    REQUIRE(as_operation(stream.at(0)).operator_name == "q");
    REQUIRE(as_operation(stream.at(0)).operands.empty());

    REQUIRE(as_operation(stream[1]).operator_name == "cm");
    REQUIRE(as_operation(stream[1]).operands.size() == 6);
    REQUIRE(as_operation(stream[1]).operands[0].as_number()->as_real() == 1.0);
    REQUIRE(as_operation(stream[1]).operands[4].as_number()->as_real() == 100.0);
    REQUIRE(as_operation(stream[1]).operands[5].as_number()->as_real() == 200.0);

    REQUIRE(as_operation(stream.at(2)).operator_name == "Q");
}

TEST_CASE("content_stream preserves operand value types", "[content][content_stream]")
{
    content_stream stream;

    stream.append(content_operation{"Tj",
                                    {object{std::int64_t{2}}, object{0.5},
                                     object{name_object{"F1"}}, object{string_object{"Hello"}}}});

    const auto& operands = as_operation(stream.at(0)).operands;
    REQUIRE(operands.size() == 4);
    REQUIRE(operands[0].is_integer());
    REQUIRE(operands[0].as_number()->as_integer() == 2);
    REQUIRE(operands[1].is_real());
    REQUIRE(operands[1].as_number()->as_real() == 0.5);
    REQUIRE(operands[2].is_name());
    REQUIRE(operands[2].as_name()->value == "F1");
    REQUIRE(operands[3].is_string());
}

TEST_CASE("content_stream stores an inline image as its own alternative",
          "[content][content_stream][inline_image]")
{
    content_stream stream;

    stream.append(make_image({std::byte{0xFF}, std::byte{0x00}, std::byte{0x10}}));

    REQUIRE(stream.size() == 1);
    REQUIRE(stream.operation_count() == 0);
    REQUIRE(stream.inline_image_count() == 1);
    REQUIRE(std::holds_alternative<inline_image>(stream.at(0)));

    const auto& image = as_image(stream.at(0));
    REQUIRE(image.dictionary.get_number("W")->as_integer() == 2);
    REQUIRE(image.dictionary.get_number("H")->as_integer() == 2);
    REQUIRE(image.dictionary.get_name("CS")->value == "DeviceRGB");
    REQUIRE(image.data ==
            std::vector<std::byte>{std::byte{0xFF}, std::byte{0x00}, std::byte{0x10}});
}

TEST_CASE("content_stream keeps instructions and inline images in stream order",
          "[content][content_stream][inline_image]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_image({std::byte{0x01}}));
    stream.append(make_operation("Q"));

    REQUIRE(stream.size() == 3);
    REQUIRE(stream.operation_count() == 2);
    REQUIRE(stream.inline_image_count() == 1);
    REQUIRE(std::holds_alternative<content_operation>(stream[0]));
    REQUIRE(std::holds_alternative<inline_image>(stream[1]));
    REQUIRE(std::holds_alternative<content_operation>(stream[2]));
}

TEST_CASE("content_stream insert places an operation at the requested index",
          "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("Q"));

    stream.insert(1, make_operation("w"));

    REQUIRE(stream.size() == 3);
    REQUIRE(as_operation(stream[0]).operator_name == "q");
    REQUIRE(as_operation(stream[1]).operator_name == "w");
    REQUIRE(as_operation(stream[2]).operator_name == "Q");
}

TEST_CASE("content_stream insert supports the beginning and the end", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    stream.insert(0, make_operation("BT"));
    stream.insert(2, make_operation("ET"));

    REQUIRE(stream.size() == 3);
    REQUIRE(as_operation(stream[0]).operator_name == "BT");
    REQUIRE(as_operation(stream[1]).operator_name == "q");
    REQUIRE(as_operation(stream[2]).operator_name == "ET");
}

TEST_CASE("content_stream insert places an inline image at the requested index",
          "[content][content_stream][inline_image]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("Q"));

    stream.insert(1, make_image({std::byte{0x42}}));

    REQUIRE(stream.size() == 3);
    REQUIRE(stream.operation_count() == 2);
    REQUIRE(stream.inline_image_count() == 1);
    REQUIRE(std::holds_alternative<inline_image>(stream[1]));
    REQUIRE(as_image(stream[1]).data == std::vector<std::byte>{std::byte{0x42}});
}

TEST_CASE("content_stream insert rejects an index past the end", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    REQUIRE_THROWS_AS(stream.insert(2, make_operation("Q")), std::out_of_range);
    REQUIRE_THROWS_AS(stream.insert(2, make_image({})), std::out_of_range);
    REQUIRE(stream.size() == 1);
}

TEST_CASE("content_stream erase removes the item at the requested index",
          "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("w"));
    stream.append(make_operation("Q"));

    stream.erase(1);

    REQUIRE(stream.size() == 2);
    REQUIRE(as_operation(stream[0]).operator_name == "q");
    REQUIRE(as_operation(stream[1]).operator_name == "Q");
}

TEST_CASE("content_stream erase removes an inline image", "[content][content_stream][inline_image]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_image({std::byte{0x01}}));

    stream.erase(1);

    REQUIRE(stream.size() == 1);
    REQUIRE(stream.inline_image_count() == 0);
}

TEST_CASE("content_stream erase rejects an out-of-range index", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    REQUIRE_THROWS_AS(stream.erase(1), std::out_of_range);
    REQUIRE(stream.size() == 1);
}

TEST_CASE("content_stream at rejects an out-of-range index", "[content][content_stream]")
{
    const content_stream stream;

    REQUIRE_THROWS_AS(stream.at(0), std::out_of_range);
}

TEST_CASE("content_stream clear removes every item", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_image({std::byte{0x01}}));
    stream.append(make_operation("Q"));

    stream.clear();

    REQUIRE(stream.empty());
    REQUIRE(stream.size() == 0);
    REQUIRE(stream.operation_count() == 0);
    REQUIRE(stream.inline_image_count() == 0);
}

TEST_CASE("content_stream copy is deep and independent", "[content][content_stream][copy]")
{
    content_stream original;
    original.append(make_operation("q"));
    original.append(make_transform(1.0, 0.0, 0.0, 1.0, 10.0, 20.0));

    content_stream copy = original;

    as_operation(copy.at(0)).operator_name = "Q";
    as_operation(copy.at(1)).operands[4] = object{99.0};
    copy.append(make_operation("S"));

    REQUIRE(original.size() == 2);
    REQUIRE(as_operation(original.at(0)).operator_name == "q");
    REQUIRE(as_operation(original.at(1)).operands[4].as_number()->as_real() == 10.0);
    REQUIRE(copy.size() == 3);
    REQUIRE(as_operation(copy.at(0)).operator_name == "Q");
    REQUIRE(as_operation(copy.at(1)).operands[4].as_number()->as_real() == 99.0);
}

TEST_CASE("content_stream copy deep-copies inline images", "[content][content_stream][copy]")
{
    content_stream original;
    original.append(make_image({std::byte{0x01}, std::byte{0x02}}));

    content_stream copy = original;
    auto& copy_image = std::get<inline_image>(copy.at(0));
    copy_image.data = {std::byte{0xAA}};
    copy_image.dictionary.set("W", object{std::int64_t{9}});

    const auto& original_image = as_image(original.at(0));
    REQUIRE(original_image.data == std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});
    REQUIRE(original_image.dictionary.get_number("W")->as_integer() == 2);
}

TEST_CASE("content_stream copy assignment is deep and independent",
          "[content][content_stream][copy]")
{
    content_stream source;
    source.append(make_operation("q"));

    content_stream target;
    target.append(make_operation("BT"));
    target = source;

    as_operation(target.at(0)).operator_name = "Q";

    REQUIRE(source.size() == 1);
    REQUIRE(as_operation(source.at(0)).operator_name == "q");
    REQUIRE(target.size() == 1);
    REQUIRE(as_operation(target.at(0)).operator_name == "Q");
}

TEST_CASE("content_stream iterates items in stream order", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("w"));
    stream.append(make_operation("Q"));

    std::vector<std::string> names;
    for (const auto& item : stream)
    {
        if (const auto* operation = std::get_if<content_operation>(&item))
        {
            names.push_back(operation->operator_name);
        }
    }

    REQUIRE(names == std::vector<std::string>{"q", "w", "Q"});

    const auto& const_stream = stream;
    REQUIRE(const_stream.begin() != const_stream.end());
    REQUIRE(const_stream.cbegin() != const_stream.cend());
}

TEST_CASE("content_stream can be constructed from an existing operation vector",
          "[content][content_stream]")
{
    std::vector<content_operation> operations;
    operations.push_back(make_operation("q"));
    operations.push_back(make_operation("Q"));

    const content_stream stream{std::move(operations)};

    REQUIRE(stream.size() == 2);
    REQUIRE(as_operation(stream.at(0)).operator_name == "q");
    REQUIRE(as_operation(stream.at(1)).operator_name == "Q");
}

TEST_CASE("content_stream can be constructed from an existing item vector",
          "[content][content_stream]")
{
    std::vector<content_item> items;
    items.push_back(make_operation("q"));
    items.push_back(make_image({std::byte{0x07}}));
    items.push_back(make_operation("Q"));

    const content_stream stream{std::move(items)};

    REQUIRE(stream.size() == 3);
    REQUIRE(stream.operation_count() == 2);
    REQUIRE(stream.inline_image_count() == 1);
    REQUIRE(as_image(stream.at(1)).data == std::vector<std::byte>{std::byte{0x07}});
}

TEST_CASE("content_stream items exposes the raw sequence", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    auto& items = stream.items();
    items.push_back(make_operation("Q"));

    REQUIRE(stream.size() == 2);
    REQUIRE(as_operation(stream.at(1)).operator_name == "Q");
}
} // namespace ripper::pdf::core
