#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
revision make_revision_from_entries(std::vector<cross_reference_entry> entries,
                                    std::optional<std::uint64_t> startxref = std::nullopt)
{
    cross_reference_subsection::entry_map map;
    for (auto& entry : entries)
    {
        map.emplace(entry.reference().object_number(), std::move(entry));
    }

    cross_reference_subsection subsection{0, std::move(map)};

    std::deque<cross_reference_subsection> subsections;
    subsections.push_back(std::move(subsection));

    cross_reference_section section{std::move(subsections), startxref};
    trailer t{dictionary_object{}};
    return revision{std::move(section), std::move(t)};
}
} // namespace

TEST_CASE("cross_reference_manager prefers newest section entries", "[xref][manager]")
{
    std::vector<cross_reference_entry> older_entries;
    older_entries.emplace_back(indirect_reference{1, 0}, 11, true);

    std::vector<cross_reference_entry> newer_entries;
    newer_entries.emplace_back(indirect_reference{1, 1}, 22, true);

    std::deque<revision> revisions;
    revisions.push_back(make_revision_from_entries(std::move(older_entries)));
    revisions.push_back(make_revision_from_entries(std::move(newer_entries)));

    revision_manager manager{std::move(revisions)};

    auto* entry = manager.xref().find(1);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->offset() == 22);
    REQUIRE(entry->reference().generation() == 1);
}

TEST_CASE("cross_reference_manager active_entries filters deleted objects", "[xref][manager]")
{
    std::vector<cross_reference_entry> entries;
    entries.emplace_back(indirect_reference{0, 65535}, 0, false);
    entries.emplace_back(indirect_reference{1, 0}, 12, false);
    entries.emplace_back(indirect_reference{2, 0}, 24, true);

    std::deque<revision> revisions;
    revisions.push_back(make_revision_from_entries(std::move(entries)));

    revision_manager manager{std::move(revisions)};
    auto active = manager.xref().active_entries();

    REQUIRE(active.contains(0));
    REQUIRE_FALSE(active.contains(1));
    REQUIRE(active.contains(2));
}

TEST_CASE("cross_reference_manager reserve appends pending entry", "[xref][manager]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));

    cross_reference_section section{std::move(subsections)};
    trailer t{dictionary_object{}};

    std::deque<revision> revisions;
    revisions.emplace_back(std::move(section), std::move(t));

    revision_manager manager{std::move(revisions)};

    const auto ref = manager.xref().reserve();
    auto* entry = manager.xref().find(ref);

    REQUIRE(ref.object_number() == 1);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->is_new());
    REQUIRE_FALSE(entry->is_resolved());
}

TEST_CASE("mark_deleted sets free fields and bumps generation", "[xref][entry][free]")
{
    cross_reference_entry entry{indirect_reference{7, 0}, 123, true};

    entry.mark_deleted();

    REQUIRE_FALSE(entry.in_use());
    REQUIRE(entry.reference().object_number() == 7);
    REQUIRE(entry.reference().generation() == 1);
    REQUIRE(entry.reuse_generation() == 1);
    REQUIRE(entry.next_free_object() == 0);
}

TEST_CASE("section free-list links deletions and recycles on take", "[xref][section][free]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});
    entries.emplace(1, cross_reference_entry{indirect_reference{1, 0}, 100, true});
    entries.emplace(2, cross_reference_entry{indirect_reference{2, 0}, 200, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(0, std::move(entries));
    cross_reference_section section{std::move(subsections)};

    section.mark_deleted(indirect_reference{2, 0});

    auto* head = section.find(0);
    REQUIRE(head != nullptr);
    REQUIRE(head->next_free_object() == 2);

    auto* freed = section.find(2);
    REQUIRE(freed != nullptr);
    REQUIRE_FALSE(freed->in_use());
    REQUIRE(freed->reuse_generation() == 1);
    REQUIRE(freed->next_free_object() == 0);

    const auto slot = section.take_free_slot();
    REQUIRE(slot.has_value());
    REQUIRE(slot->object_number() == 2);
    REQUIRE(slot->generation() == 1);
    REQUIRE(section.find(2) == nullptr);
    REQUIRE(head->next_free_object() == 0);
}

TEST_CASE("cross_reference_manager recycles freed slots", "[xref][manager][free]")
{
    std::vector<cross_reference_entry> entries;
    entries.emplace_back(indirect_reference{0, 65535}, 0, false);
    entries.emplace_back(indirect_reference{3, 0}, 30, true);
    entries.emplace_back(indirect_reference{5, 0}, 50, true);

    std::deque<revision> revisions;
    revisions.push_back(make_revision_from_entries(std::move(entries)));

    revision_manager manager{std::move(revisions)};
    auto& xref = manager.xref();

    // Free 5 first, then 3, so the list threads as head -> 3 -> 5 -> 0.
    xref.mark_deleted(indirect_reference{5, 0});
    xref.mark_deleted(indirect_reference{3, 0});

    const auto reserve_ref = xref.reserve();
    REQUIRE(reserve_ref.object_number() == 3);
    REQUIRE(reserve_ref.generation() == 1);

    auto* pending = xref.find(3);
    REQUIRE(pending != nullptr);
    REQUIRE(pending->is_new());
    REQUIRE(pending->reference().generation() == 1);

    document doc{nullptr, nullptr};
    const auto alloc_ref = xref.allocate(std::make_unique<indirect_object>(
        object_identity{&doc, indirect_reference{5, 0}}, object{dictionary_object{}}));
    REQUIRE(alloc_ref.object_number() == 5);
    REQUIRE(alloc_ref.generation() == 1);

    auto* allocated = xref.find(5);
    REQUIRE(allocated != nullptr);
    REQUIRE(allocated->in_use());
    REQUIRE(allocated->reference().generation() == 1);
}

TEST_CASE("cross_reference_manager records deletion in active section", "[xref][manager][free]")
{
    std::vector<cross_reference_entry> old_entries;
    old_entries.emplace_back(indirect_reference{0, 65535}, 0, false);
    old_entries.emplace_back(indirect_reference{9, 0}, 90, true);

    std::deque<revision> revisions;
    revisions.push_back(make_revision_from_entries(std::move(old_entries)));

    cross_reference_subsection::entry_map head_entries;
    head_entries.emplace(0, cross_reference_entry{indirect_reference{0, 65535}, 0, false});
    std::deque<cross_reference_subsection> subs;
    subs.emplace_back(0, std::move(head_entries));
    revisions.emplace_back(cross_reference_section{std::move(subs)}, trailer{dictionary_object{}});

    revision_manager manager{std::move(revisions)};
    auto& xref = manager.xref();

    xref.mark_deleted(indirect_reference{9, 0});

    // Newest-first lookup: the free entry in the active section shadows obj 9.
    auto* entry = xref.find(9);
    REQUIRE(entry != nullptr);
    REQUIRE_FALSE(entry->in_use());
    REQUIRE(entry->reference().object_number() == 9);
    REQUIRE(entry->reuse_generation() == 1);

    // The older on-disk revision must remain untouched.
    auto* old_entry = manager.all()[0].section().find(9);
    REQUIRE(old_entry != nullptr);
    REQUIRE(old_entry->in_use());

    // The free entry is threaded through the active section head.
    auto* head = manager.all()[1].section().find(0);
    REQUIRE(head != nullptr);
    REQUIRE(head->next_free_object() == 9);
}
} // namespace ripper::pdf::core
