#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace ripper::pdf::core
{
// A document is movable (ownership transfer that rebinds every internal
// back-reference) but not copyable. The reader would have to be cloned and the
// writer is exclusive and stateful, so no single copy policy fits every use.
static_assert(!std::is_copy_constructible_v<document>);
static_assert(!std::is_copy_assignable_v<document>);
static_assert(std::is_move_constructible_v<document>);
static_assert(std::is_move_assignable_v<document>);
static_assert(std::is_nothrow_move_constructible_v<document>);
static_assert(std::is_nothrow_move_assignable_v<document>);

// Constructing a string object allocates, so it must be allowed to propagate
// allocation failures rather than terminating.
static_assert(!std::is_nothrow_constructible_v<object, std::string>);

TEST_CASE("document factories produce movable-only documents", "[document][ownership]")
{
    document doc{nullptr, nullptr};
    REQUIRE(doc.resource_limits().max_parse_depth > 0);

    document moved{std::move(doc)};
    REQUIRE_FALSE(moved.has_reader());
}
} // namespace ripper::pdf::core
