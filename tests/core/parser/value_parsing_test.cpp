#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/value_parsing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

namespace ripper::pdf::core
{
TEST_CASE("parse_indirect_reference parses object and generation", "[parser][value]")
{
    pdf_lexer lexer{"42 7 R"};

    const auto ref = parse_indirect_reference(lexer);

    REQUIRE(ref.object_number() == 42);
    REQUIRE(ref.generation() == 7);
}

TEST_CASE("parse_value enforces a nesting depth limit", "[parser][value][limits]")
{
    std::string nested(300, '[');
    nested.append(300, ']');
    pdf_lexer lexer{nested};

    REQUIRE_THROWS_AS((void)parse_value(lexer), parse_exception);

    pdf_lexer shallow{"[[[[[]]]]]"};
    REQUIRE_NOTHROW((void)parse_value(shallow, 5));

    pdf_lexer too_shallow{"[[[[[]]]]]"};
    REQUIRE_THROWS_AS((void)parse_value(too_shallow, 3), parse_exception);
}

TEST_CASE("parse_value detects indirect references", "[parser][value]")
{
    pdf_lexer lexer{"15 0 R"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_indirect_reference());
    REQUIRE(value.as_indirect_reference()->object_number() == 15);
    REQUIRE(value.as_indirect_reference()->generation() == 0);
}

TEST_CASE("parse_value parses nested dictionaries and arrays", "[parser][value]")
{
    pdf_lexer lexer{"<< /Count 2 /Kids [1 0 R 2 0 R] /Type /Pages >>"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_dictionary());
    const auto* dict = value.as_dictionary();
    REQUIRE(dict != nullptr);

    const auto* count = dict->get_number("Count");
    REQUIRE(count != nullptr);
    REQUIRE(count->as_integer() == 2);

    const auto* kids = dict->get_array("Kids");
    REQUIRE(kids != nullptr);
    REQUIRE(kids->size() == 2);
    REQUIRE((*kids)[0].is_indirect_reference());
    REQUIRE((*kids)[1].is_indirect_reference());

    const auto* type = dict->get_name("Type");
    REQUIRE(type != nullptr);
    REQUIRE(type->value == "Pages");
}

TEST_CASE("parse_indirect_reference validates malformed values", "[parser][value]")
{
    pdf_lexer malformed{"7 R"};
    REQUIRE_THROWS_AS((void)parse_indirect_reference(malformed), parse_exception);

    pdf_lexer out_of_range{"4294967296 0 R"};
    REQUIRE_THROWS_AS((void)parse_indirect_reference(out_of_range), parse_exception);
}

TEST_CASE("parse_value unescapes hex in names", "[parser][value][name]")
{
    pdf_lexer lexer{"/A#28B#29C"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_name());
    const auto* n = value.as_name();
    REQUIRE(n != nullptr);
    REQUIRE(n->value == "A(B)C");
}

TEST_CASE("parse_value unescapes space in names", "[parser][value][name]")
{
    pdf_lexer lexer{"/Foo#20Bar"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_name());
    REQUIRE(value.as_name()->value == "Foo Bar");
}

TEST_CASE("parse_value unescapes hash in names", "[parser][value][name]")
{
    pdf_lexer lexer{"/Version#231"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_name());
    REQUIRE(value.as_name()->value == "Version#1");
}

TEST_CASE("parse_value unescapes non-ASCII in names", "[parser][value][name]")
{
    pdf_lexer lexer{"/caf#C3#A9"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_name());
    REQUIRE(value.as_name()->value == "caf\xC3\xA9");
}

TEST_CASE("parse_value preserves literal hash when not followed by hex", "[parser][value][name]")
{
    pdf_lexer lexer{"/ab#ZZ"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_name());
    REQUIRE(value.as_name()->value == "ab#ZZ");
}

TEST_CASE("parse_value parses dictionary_object with escaped name keys", "[parser][value][name]")
{
    pdf_lexer lexer{"<< /A#20Key /Value >>"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_dictionary());
    const auto* dict = value.as_dictionary();
    REQUIRE(dict != nullptr);
    REQUIRE(dict->contains("A Key"));
    const auto* v = dict->get_name("A Key");
    REQUIRE(v != nullptr);
    REQUIRE(v->value == "Value");
}

TEST_CASE("parse_value decodes hex strings", "[parser][value][string]")
{
    SECTION("valid even-length hex string")
    {
        pdf_lexer lexer{"<414243>"};

        const auto value = parse_value(lexer);

        REQUIRE(value.is_string());
        const auto* s = value.as_string();
        REQUIRE(s != nullptr);
        REQUIRE(s->is_hex());
        REQUIRE(s->as_string() == "ABC");
    }

    SECTION("odd number of hex digits pads trailing nibble with zero")
    {
        pdf_lexer lexer{"<414>"};

        const auto value = parse_value(lexer);

        REQUIRE(value.is_string());
        REQUIRE(value.as_string()->as_string() == std::string{"A\x40", 2});
    }

    SECTION("whitespace between hex digits is ignored")
    {
        pdf_lexer lexer{"<41 42 43>"};

        const auto value = parse_value(lexer);

        REQUIRE(value.is_string());
        REQUIRE(value.as_string()->as_string() == "ABC");
    }

    SECTION("invalid hex digit throws")
    {
        pdf_lexer lexer{"<41Z2>"};

        REQUIRE_THROWS_AS((void)parse_value(lexer), parse_exception);
    }
}

TEST_CASE("parse_value roundtrips escaped names through serializer", "[parser][value][name]")
{
    pdf_lexer lexer{"<< /Key#28parens#29 /#3C#3E /Percent#25 /Slash#2F >>"};

    const auto value = parse_value(lexer);

    REQUIRE(value.is_dictionary());
    const auto* dict = value.as_dictionary();
    REQUIRE(dict != nullptr);

    const auto* parens_key = dict->get_name("Key(parens)");
    REQUIRE(parens_key != nullptr);
    REQUIRE(parens_key->value == "<>");

    const auto* percent_key = dict->get_name("Percent%");
    REQUIRE(percent_key != nullptr);
    REQUIRE(percent_key->value == "Slash/");
}
} // namespace ripper::pdf::core
