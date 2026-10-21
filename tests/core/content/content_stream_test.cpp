#include "ripper/pdf/core/content/content_stream.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] content_operation make_operation(std::string operator_name,
                                               std::vector<object> operands = {})
{
    return content_operation{std::move(operands), std::move(operator_name)};
}

[[nodiscard]] content_operation make_transform(double a, double b, double c, double d, double e,
                                               double f)
{
    return content_operation{{object{a}, object{b}, object{c}, object{d}, object{e}, object{f}},
                             "cm"};
}
} // namespace

TEST_CASE("a new content_stream is empty", "[content][content_stream]")
{
    const content_stream stream;

    REQUIRE(stream.empty());
    REQUIRE(stream.size() == 0);
    REQUIRE(stream.operations().empty());
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

    REQUIRE(stream.at(0).operator_name == "q");
    REQUIRE(stream.at(0).operands.empty());

    REQUIRE(stream[1].operator_name == "cm");
    REQUIRE(stream[1].operands.size() == 6);
    REQUIRE(stream[1].operands[0].as_number()->as_real() == 1.0);
    REQUIRE(stream[1].operands[4].as_number()->as_real() == 100.0);
    REQUIRE(stream[1].operands[5].as_number()->as_real() == 200.0);

    REQUIRE(stream.at(2).operator_name == "Q");
}

TEST_CASE("content_stream preserves operand value types", "[content][content_stream]")
{
    content_stream stream;

    stream.append(content_operation{{object{std::int64_t{2}}, object{0.5},
                                     object{name_object{"F1"}}, object{string_object{"Hello"}}},
                                    "Tj"});

    const auto& operands = stream.at(0).operands;
    REQUIRE(operands.size() == 4);
    REQUIRE(operands[0].is_integer());
    REQUIRE(operands[0].as_number()->as_integer() == 2);
    REQUIRE(operands[1].is_real());
    REQUIRE(operands[1].as_number()->as_real() == 0.5);
    REQUIRE(operands[2].is_name());
    REQUIRE(operands[2].as_name()->value == "F1");
    REQUIRE(operands[3].is_string());
}

TEST_CASE("content_stream insert places an operation at the requested index",
          "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("Q"));

    stream.insert(1, make_operation("w"));

    REQUIRE(stream.size() == 3);
    REQUIRE(stream[0].operator_name == "q");
    REQUIRE(stream[1].operator_name == "w");
    REQUIRE(stream[2].operator_name == "Q");
}

TEST_CASE("content_stream insert supports the beginning and the end", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    stream.insert(0, make_operation("BT"));
    stream.insert(2, make_operation("ET"));

    REQUIRE(stream.size() == 3);
    REQUIRE(stream[0].operator_name == "BT");
    REQUIRE(stream[1].operator_name == "q");
    REQUIRE(stream[2].operator_name == "ET");
}

TEST_CASE("content_stream insert rejects an index past the end", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    REQUIRE_THROWS_AS(stream.insert(2, make_operation("Q")), std::out_of_range);
    REQUIRE(stream.size() == 1);
}

TEST_CASE("content_stream erase removes the operation at the requested index",
          "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("w"));
    stream.append(make_operation("Q"));

    stream.erase(1);

    REQUIRE(stream.size() == 2);
    REQUIRE(stream[0].operator_name == "q");
    REQUIRE(stream[1].operator_name == "Q");
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

TEST_CASE("content_stream clear removes every operation", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("Q"));

    stream.clear();

    REQUIRE(stream.empty());
    REQUIRE(stream.size() == 0);
}

TEST_CASE("content_stream copy is deep and independent", "[content][content_stream][copy]")
{
    content_stream original;
    original.append(make_operation("q"));
    original.append(make_transform(1.0, 0.0, 0.0, 1.0, 10.0, 20.0));

    content_stream copy = original;

    copy.at(0).operator_name = "Q";
    copy.at(1).operands[4] = object{99.0};
    copy.append(make_operation("S"));

    REQUIRE(original.size() == 2);
    REQUIRE(original.at(0).operator_name == "q");
    REQUIRE(original.at(1).operands[4].as_number()->as_real() == 10.0);
    REQUIRE(copy.size() == 3);
    REQUIRE(copy.at(0).operator_name == "Q");
    REQUIRE(copy.at(1).operands[4].as_number()->as_real() == 99.0);
}

TEST_CASE("content_stream copy assignment is deep and independent",
          "[content][content_stream][copy]")
{
    content_stream source;
    source.append(make_operation("q"));

    content_stream target;
    target.append(make_operation("BT"));
    target = source;

    target.at(0).operator_name = "Q";

    REQUIRE(source.size() == 1);
    REQUIRE(source.at(0).operator_name == "q");
    REQUIRE(target.size() == 1);
    REQUIRE(target.at(0).operator_name == "Q");
}

TEST_CASE("content_stream iterates operations in stream order", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));
    stream.append(make_operation("w"));
    stream.append(make_operation("Q"));

    std::vector<std::string> names;
    for (const auto& operation : stream)
    {
        names.push_back(operation.operator_name);
    }

    REQUIRE(names == std::vector<std::string>{"q", "w", "Q"});

    const auto& const_stream = stream;
    REQUIRE(const_stream.begin() != const_stream.end());
    REQUIRE(const_stream.cbegin() != const_stream.cend());
}

TEST_CASE("content_stream can be constructed from an existing operation vector",
          "[content][content_stream]")
{
    content_stream::storage_type operations;
    operations.push_back(make_operation("q"));
    operations.push_back(make_operation("Q"));

    const content_stream stream{std::move(operations)};

    REQUIRE(stream.size() == 2);
    REQUIRE(stream.at(0).operator_name == "q");
    REQUIRE(stream.at(1).operator_name == "Q");
}

TEST_CASE("content_stream operations exposes the raw vector", "[content][content_stream]")
{
    content_stream stream;
    stream.append(make_operation("q"));

    auto& operations = stream.operations();
    operations.push_back(make_operation("Q"));

    REQUIRE(stream.size() == 2);
    REQUIRE(stream.at(1).operator_name == "Q");
}
} // namespace ripper::pdf::core
