#include "ripper/pdf/core/util/text.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ripper::pdf::core::text_utils
{
// ── escape_name / unescape_name roundtrip ─────────────────────────────────

TEST_CASE("escape_name roundtrips simple printable names", "[util][text][name]")
{
    const std::string_view inputs[] = {
        "Type", "Pages", "Catalog", "HelloWorld", "abcXYZ123", "A(B)C",       "test#value",
        "a/b",  "<tag>", "[0]",     "{key}",      "50%",       "hello world",
    };

    for (auto input : inputs)
    {
        const auto escaped = escape_name(input);
        const auto unescaped = unescape_name(escaped);
        REQUIRE(unescaped == input);
    }
}

TEST_CASE("escape_name roundtrips non-ASCII names", "[util][text][name]")
{
    const auto input = std::string{"caf\xC3\xA9 resum\xC3\xA9"};
    const auto escaped = escape_name(input);
    const auto unescaped = unescape_name(escaped);
    REQUIRE(unescaped == input);
}

TEST_CASE("escape_name roundtrips name with all special chars", "[util][text][name]")
{
    const std::string input{"#/()<>[]{}%"};
    const auto escaped = escape_name(input);
    const auto unescaped = unescape_name(escaped);
    REQUIRE(unescaped == input);
}

TEST_CASE("unescape_name handles lone hash at end", "[util][text][name]")
{
    const auto result = unescape_name("value#");
    REQUIRE(result == "value#");
}

TEST_CASE("unescape_name handles lone hash not followed by hex", "[util][text][name]")
{
    const auto result = unescape_name("ab#ZZc");
    REQUIRE(result == "ab#ZZc");
}

TEST_CASE("unescape_name handles hash with single hex digit", "[util][text][name]")
{
    const auto result = unescape_name("ab#Az");
    REQUIRE(result == "ab#Az");
}

TEST_CASE("unescape_name handles empty string", "[util][text][name]")
{
    const auto result = unescape_name("");
    REQUIRE(result.empty());
}

TEST_CASE("escape_name handles empty string", "[util][text][name]")
{
    const auto result = escape_name("");
    REQUIRE(result.empty());
}

// ── escape_literal_string / unescape_literal_string roundtrip ────────────

TEST_CASE("unescape_literal_string roundtrips with escape_literal_string", "[util][text][string]")
{
    const std::string_view inputs[] = {
        "Hello World", "a(b)c",       "path\\to\\file", "line\nbreak",         "tab\there",
        "return\rand", "back\bspace", "form\ffeed",     "mix\nof\rall\tchars",
    };

    for (auto input : inputs)
    {
        const auto escaped = escape_literal_string(input);
        const auto unescaped = unescape_literal_string(escaped);
        REQUIRE(unescaped == input);
    }
}

TEST_CASE("unescape_literal_string handles escaped newlines", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\nb");
    REQUIRE(result == "a\nb");
}

TEST_CASE("unescape_literal_string handles escaped carriage return", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\rb");
    REQUIRE(result == "a\rb");
}

TEST_CASE("unescape_literal_string handles escaped tab", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\tb");
    REQUIRE(result == "a\tb");
}

TEST_CASE("unescape_literal_string handles escaped backspace", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\bb");
    REQUIRE(result == "a\bb");
}

TEST_CASE("unescape_literal_string handles escaped form feed", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\fb");
    REQUIRE(result == "a\fb");
}

TEST_CASE("unescape_literal_string handles octal escapes", "[util][text][string]")
{
    SECTION("single octal digit")
    {
        const auto result = unescape_literal_string("\\101");
        REQUIRE(result == std::string(1, '\101'));
    }

    SECTION("three octal digits")
    {
        const auto result = unescape_literal_string("\\101\\102\\103");
        REQUIRE(result == "ABC");
    }

    SECTION("octal 0")
    {
        const auto result = unescape_literal_string("\\0");
        REQUIRE(result == std::string(1, '\0'));
    }

    SECTION("octal 377")
    {
        const auto result = unescape_literal_string("\\377");
        REQUIRE(result == std::string(1, '\377'));
    }
}

TEST_CASE("unescape_literal_string handles line continuation", "[util][text][string]")
{
    const auto result = unescape_literal_string("line\\\ncontinued");
    REQUIRE(result == "linecontinued");
}

TEST_CASE("unescape_literal_string handles unknown escape sequences", "[util][text][string]")
{
    const auto result = unescape_literal_string("a\\zb");
    REQUIRE(result == "azb");
}

TEST_CASE("unescape_literal_string handles empty string", "[util][text][string]")
{
    const auto result = unescape_literal_string("");
    REQUIRE(result.empty());
}

TEST_CASE("unescape_literal_string handles string without escapes", "[util][text][string]")
{
    const auto result = unescape_literal_string("plain text");
    REQUIRE(result == "plain text");
}

TEST_CASE("unescape_literal_string handles trailing backslash", "[util][text][string]")
{
    const auto result = unescape_literal_string("trailing\\");
    REQUIRE(result == "trailing\\");
}

// ── escape_name encoding specifics ───────────────────────────────────────

TEST_CASE("escape_name encodes space as #20", "[util][text][name]")
{
    REQUIRE(escape_name(" ") == "#20");
}

TEST_CASE("escape_name encodes null byte", "[util][text][name]")
{
    std::string input;
    input += '\0';
    REQUIRE(escape_name(input) == "#00");
}

TEST_CASE("escape_name encodes DEL as #7F", "[util][text][name]")
{
    std::string input;
    input += '\x7F';
    REQUIRE(escape_name(input) == "#7F");
}

TEST_CASE("escape_name does not escape printable ASCII except specials", "[util][text][name]")
{
    REQUIRE(escape_name("azAZ09!\"$&'*+,-.:;=?@\\^_`|~") == "azAZ09!\"$&'*+,-.:;=?@\\^_`|~");
}

// ── unescape_name hex decoding ───────────────────────────────────────────

TEST_CASE("unescape_name decodes uppercase hex", "[util][text][name]")
{
    const auto result = unescape_name("A#41B");
    REQUIRE(result == "AAB");
}

TEST_CASE("unescape_name decodes lowercase hex", "[util][text][name]")
{
    const auto result = unescape_name("A#61B");
    REQUIRE(result == "AaB");
}

TEST_CASE("unescape_name decodes multiple hex sequences", "[util][text][name]")
{
    const auto result = unescape_name("#48#65#6C#6C#6F");
    REQUIRE(result == "Hello");
}

// ── starts_with_token word boundary ───────────────────────────────────────

TEST_CASE("starts_with_token requires a token boundary after the keyword", "[util][text][token]")
{
    REQUIRE(starts_with_token("startxref 42", "startxref"));
    REQUIRE(starts_with_token("startxref\n42", "startxref"));
    REQUIRE(starts_with_token("startxref", "startxref"));
    REQUIRE(starts_with_token("  xref ", "xref"));
    REQUIRE(starts_with_token("trailer\n<<", "trailer"));

    REQUIRE_FALSE(starts_with_token("startxrefGarbage", "startxref"));
    REQUIRE_FALSE(starts_with_token("startxref42", "startxref"));
    REQUIRE_FALSE(starts_with_token("startxrefx", "startxref"));
    REQUIRE_FALSE(starts_with_token("xrefy", "xref"));
    REQUIRE_FALSE(starts_with_token("trailerfoo", "trailer"));
}

TEST_CASE("starts_with_token trims surrounding whitespace and line endings", "[util][text][token]")
{
    REQUIRE(starts_with_token("\n\n startxref 7", "startxref"));
    REQUIRE(starts_with_token("startxref 7 \r\n", "startxref"));
    REQUIRE_FALSE(starts_with_token("", "startxref"));
    REQUIRE_FALSE(starts_with_token("start", "startxref"));
}
} // namespace ripper::pdf::core::text_utils
