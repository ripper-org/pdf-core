#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/filter/filter_manager.hpp"
#include "ripper/pdf/core/filter/flate_decode_filter.hpp"
#include "ripper/pdf/core/filter/lzw_decode_filter.hpp"
#include "ripper/pdf/core/filter/predictor.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> zeros(std::size_t size)
{
    return std::vector<std::byte>(size, std::byte{0});
}
} // namespace

TEST_CASE("flate_decode_filter stops at the decoded size limit", "[filter][flate][limits]")
{
    flate_decode_filter filter;
    auto input = zeros(64U * 1024U);
    auto compressed = filter.encode(input);

    resource_limits limits;
    limits.max_decoded_stream_bytes = 1024;

    REQUIRE_THROWS_AS(filter.decode_with_limits(compressed, nullptr, limits), parse_exception);

    limits.max_decoded_stream_bytes = input.size();
    REQUIRE(filter.decode_with_limits(compressed, nullptr, limits) == input);
}

TEST_CASE("lzw_decode_filter stops at the decoded size limit", "[filter][lzw][limits]")
{
    lzw_decode_filter filter;
    auto input = zeros(64U * 1024U);
    auto compressed = filter.encode(input);

    resource_limits limits;
    limits.max_decoded_stream_bytes = 1024;

    REQUIRE_THROWS_AS(filter.decode_with_limits(compressed, nullptr, limits), parse_exception);

    limits.max_decoded_stream_bytes = input.size();
    REQUIRE(filter.decode_with_limits(compressed, nullptr, limits) == input);
}

TEST_CASE("filter_manager bounds filter chains and unfiltered data", "[filter][manager][limits]")
{
    auto input = zeros(64U * 1024U);

    dictionary_object filtered;
    filtered.set("Filter", object{name_object{"FlateDecode"}});
    const auto compressed = filter_manager::encode(filtered, input);

    resource_limits limits;
    limits.max_decoded_stream_bytes = 1024;

    REQUIRE_THROWS_AS(filter_manager::decode(filtered, compressed, limits), parse_exception);

    dictionary_object unfiltered;
    REQUIRE_THROWS_AS(filter_manager::decode(unfiltered, input, limits), parse_exception);
}

TEST_CASE("apply_predictor rejects rows larger than the configured limit",
          "[filter][predictor][limits]")
{
    dictionary_object params;
    params.set("Predictor", object{std::int64_t{12}});
    params.set("Columns", object{std::int64_t{1'000'000}});
    params.set("Colors", object{std::int64_t{1}});
    params.set("BitsPerComponent", object{std::int64_t{8}});

    resource_limits limits;
    limits.max_predictor_row_bytes = 1024;

    REQUIRE_THROWS_AS(apply_predictor(zeros(1'000'001), params, limits), parse_exception);
}

TEST_CASE("apply_predictor validates input length before allocating rows",
          "[filter][predictor][bounds]")
{
    dictionary_object params;
    params.set("Predictor", object{std::int64_t{12}});
    params.set("Columns", object{std::int64_t{4}});
    params.set("Colors", object{std::int64_t{1}});
    params.set("BitsPerComponent", object{std::int64_t{8}});

    // A row needs one tag byte plus four data bytes; four bytes are truncated.
    REQUIRE_THROWS_AS(apply_predictor(zeros(4), params), parse_exception);

    REQUIRE_NOTHROW(apply_predictor(zeros(5), params));
}

TEST_CASE("apply_predictor rejects overflowing row dimensions", "[filter][predictor][bounds]")
{
    dictionary_object params;
    params.set("Predictor", object{std::int64_t{12}});
    params.set("Columns", object{std::numeric_limits<std::int64_t>::max()});
    params.set("Colors", object{std::numeric_limits<std::int64_t>::max()});
    params.set("BitsPerComponent", object{std::int64_t{8}});

    REQUIRE_THROWS_AS(apply_predictor(zeros(5), params), parse_exception);
}

TEST_CASE("apply_predictor rejects negative parameters", "[filter][predictor][bounds]")
{
    dictionary_object params;
    params.set("Predictor", object{std::int64_t{12}});
    params.set("Columns", object{std::int64_t{-1}});

    REQUIRE_THROWS_AS(apply_predictor(zeros(5), params), parse_exception);
}
} // namespace ripper::pdf::core
