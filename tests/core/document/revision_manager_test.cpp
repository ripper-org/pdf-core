#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <deque>
#include <optional>
#include <type_traits>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
revision make_revision(std::uint64_t offset, std::int64_t size)
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});
    entries.emplace(1, cross_reference_entry{indirect_reference{1, 0}, offset, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));

    dictionary_object trailer_dict;
    trailer_dict.set("Size", object{size});

    return revision{cross_reference_section{std::move(subsections)},
                    trailer{std::move(trailer_dict)}};
}

std::deque<revision> make_revisions(std::uint64_t offset, std::int64_t size)
{
    std::deque<revision> revisions;
    revisions.push_back(make_revision(offset, size));
    return revisions;
}
} // namespace

static_assert(std::is_copy_constructible_v<revision_manager>);
static_assert(std::is_move_constructible_v<revision_manager>);
static_assert(std::is_copy_assignable_v<revision_manager>);
static_assert(std::is_move_assignable_v<revision_manager>);

TEST_CASE("revision_manager copy binds views to the copied revisions",
          "[document][revision_manager]")
{
    revision_manager original{make_revisions(11, 1)};

    auto copy = original;
    copy.trailer().active_trailer().dictionary().set("Marker", object{std::int64_t{7}});

    REQUIRE(copy.trailer().active_trailer().dictionary().contains("Marker"));
    REQUIRE_FALSE(original.trailer().active_trailer().dictionary().contains("Marker"));
    REQUIRE(copy.xref().find(1)->offset() == 11);
}

TEST_CASE("revision_manager copy remains usable after the source is destroyed",
          "[document][revision_manager]")
{
    std::optional<revision_manager> copy;

    {
        revision_manager original{make_revisions(21, 2)};
        copy.emplace(original);
    }

    REQUIRE(copy->size() == 1);
    REQUIRE(copy->xref().find(1)->offset() == 21);
    REQUIRE(copy->trailer().active_trailer().dictionary().get_number("Size")->as_integer() == 2);
}

TEST_CASE("revision_manager move transfers views to the destination",
          "[document][revision_manager]")
{
    std::optional<revision_manager> moved;

    {
        revision_manager original{make_revisions(31, 3)};
        moved.emplace(std::move(original));
    }

    REQUIRE(moved->size() == 1);
    REQUIRE(moved->xref().find(1)->offset() == 31);
    REQUIRE(moved->current().section().find(1)->offset() == 31);
}

TEST_CASE("revision_manager move assignment replaces the destination",
          "[document][revision_manager]")
{
    revision_manager source{make_revisions(41, 4)};
    revision_manager destination{make_revisions(51, 5)};

    destination = std::move(source);

    REQUIRE(destination.size() == 1);
    REQUIRE(destination.xref().find(1)->offset() == 41);
    REQUIRE(destination.trailer().active_trailer().dictionary().get_number("Size")->as_integer() ==
            4);
}

TEST_CASE("moved-from revision_manager reports an empty history", "[document][revision_manager]")
{
    revision_manager source{make_revisions(61, 6)};
    const revision_manager moved{std::move(source)};

    REQUIRE(moved.size() == 1);
    REQUIRE(source.all().empty());

    REQUIRE_THROWS_AS(source.current(), logic_exception);
    REQUIRE_THROWS_AS(source.xref().active_section(), logic_exception);
    REQUIRE_THROWS_AS(source.trailer().active_trailer(), logic_exception);
}
} // namespace ripper::pdf::core
