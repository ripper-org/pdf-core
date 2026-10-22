#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>

namespace ripper::pdf::core
{
// A document's objects hold a back-reference to the document itself, so the
// facade must not be relocated or duplicated.
static_assert(!std::is_copy_constructible_v<document>);
static_assert(!std::is_copy_assignable_v<document>);
static_assert(!std::is_move_constructible_v<document>);
static_assert(!std::is_move_assignable_v<document>);

// Constructing a string object allocates, so it must be allowed to propagate
// allocation failures rather than terminating.
static_assert(!std::is_nothrow_constructible_v<object, std::string>);

TEST_CASE("document factories still produce documents without move support",
          "[document][ownership]")
{
    document doc{nullptr, nullptr};
    REQUIRE(doc.limits().max_parse_depth > 0);
}
} // namespace ripper::pdf::core
