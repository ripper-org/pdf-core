#include "ripper/pdf/core/content/operator.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string_view>

namespace ripper::pdf::core
{
namespace
{
using namespace content_operator;

struct operator_expectation
{
    std::string_view name;
    std::string_view actual;
    std::string_view expected;
};

constexpr auto operator_expectations = std::array{
    // General graphics state (§8.4)
    operator_expectation{"w", w, "w"},
    operator_expectation{"J", J, "J"},
    operator_expectation{"j", j, "j"},
    operator_expectation{"M", M, "M"},
    operator_expectation{"d", d, "d"},
    operator_expectation{"ri", ri, "ri"},
    operator_expectation{"i", i, "i"},
    operator_expectation{"gs", gs, "gs"},
    operator_expectation{"q", q, "q"},
    operator_expectation{"Q", Q, "Q"},
    operator_expectation{"cm", cm, "cm"},
    // Path construction (§8.5.2)
    operator_expectation{"m", m, "m"},
    operator_expectation{"l", l, "l"},
    operator_expectation{"c", c, "c"},
    operator_expectation{"v", v, "v"},
    operator_expectation{"y", y, "y"},
    operator_expectation{"h", h, "h"},
    operator_expectation{"re", re, "re"},
    // Path painting (§8.5.3)
    operator_expectation{"S", S, "S"},
    operator_expectation{"s", s, "s"},
    operator_expectation{"f", f, "f"},
    operator_expectation{"F", F, "F"},
    operator_expectation{"f_star", f_star, "f*"},
    operator_expectation{"B", B, "B"},
    operator_expectation{"B_star", B_star, "B*"},
    operator_expectation{"b", b, "b"},
    operator_expectation{"b_star", b_star, "b*"},
    operator_expectation{"n", n, "n"},
    // Clipping (§8.5.4)
    operator_expectation{"W", W, "W"},
    operator_expectation{"W_star", W_star, "W*"},
    // Color (§8.6.8)
    operator_expectation{"cs", cs, "cs"},
    operator_expectation{"CS", CS, "CS"},
    operator_expectation{"sc", sc, "sc"},
    operator_expectation{"SC", SC, "SC"},
    operator_expectation{"scn", scn, "scn"},
    operator_expectation{"SCN", SCN, "SCN"},
    operator_expectation{"g", g, "g"},
    operator_expectation{"G", G, "G"},
    operator_expectation{"rg", rg, "rg"},
    operator_expectation{"RG", RG, "RG"},
    operator_expectation{"k", k, "k"},
    operator_expectation{"K", K, "K"},
    // Text state (§9.3)
    operator_expectation{"Tc", Tc, "Tc"},
    operator_expectation{"Tw", Tw, "Tw"},
    operator_expectation{"Tz", Tz, "Tz"},
    operator_expectation{"TL", TL, "TL"},
    operator_expectation{"Tf", Tf, "Tf"},
    operator_expectation{"Tr", Tr, "Tr"},
    operator_expectation{"Ts", Ts, "Ts"},
    // Text object and positioning (§9.4)
    operator_expectation{"BT", BT, "BT"},
    operator_expectation{"ET", ET, "ET"},
    operator_expectation{"Td", Td, "Td"},
    operator_expectation{"TD", TD, "TD"},
    operator_expectation{"Tm", Tm, "Tm"},
    operator_expectation{"T_star", T_star, "T*"},
    operator_expectation{"Tj", Tj, "Tj"},
    operator_expectation{"TJ", TJ, "TJ"},
    operator_expectation{"single_quote", single_quote, "'"},
    operator_expectation{"double_quote", double_quote, "\""},
    // XObjects (§8.10)
    operator_expectation{"Do", Do, "Do"},
    // Inline images (§8.9.7)
    operator_expectation{"BI", BI, "BI"},
    operator_expectation{"ID", ID, "ID"},
    operator_expectation{"EI", EI, "EI"},
    // Marked content (§14.6)
    operator_expectation{"MP", MP, "MP"},
    operator_expectation{"DP", DP, "DP"},
    operator_expectation{"BMC", BMC, "BMC"},
    operator_expectation{"BDC", BDC, "BDC"},
    operator_expectation{"EMC", EMC, "EMC"},
    // Compatibility (§14.7)
    operator_expectation{"BX", BX, "BX"},
    operator_expectation{"EX", EX, "EX"},
    // Shading (§8.7)
    operator_expectation{"sh", sh, "sh"},
    // Type 3 font metrics (§9.6.6)
    operator_expectation{"d0", d0, "d0"},
    operator_expectation{"d1", d1, "d1"},
};
} // namespace

TEST_CASE("content operator constants match their PDF syntax", "[content][operator]")
{
    for (const auto& expectation : operator_expectations)
    {
        INFO("constant: " << expectation.name);
        REQUIRE(expectation.actual == expectation.expected);
    }
}

TEST_CASE("content operator constants carry no delimiters or whitespace", "[content][operator]")
{
    for (const auto& expectation : operator_expectations)
    {
        INFO("constant: " << expectation.name);
        REQUIRE_FALSE(expectation.actual.empty());
        REQUIRE(expectation.actual.front() != '/');
        REQUIRE(expectation.actual.front() != ' ');
        REQUIRE(expectation.actual.back() != ' ');
    }
}
} // namespace ripper::pdf::core
