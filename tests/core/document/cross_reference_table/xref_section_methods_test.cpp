#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace ripper::pdf::core
{

indirect_object make_obj(document& doc, std::uint32_t num, std::string value)
{
    dictionary_object dict;
    dict.set("Data", object{std::move(value)});
    return indirect_object{object_identity{&doc, indirect_reference{num, 0}},
                           object{std::move(dict)}};
}

TEST_CASE("add_entry_from copies a resolved entry into a section",
          "[cross_reference_section][add_entry_from]")
{
    document doc{nullptr, nullptr};

    cross_reference_subsection::entry_map entries;
    auto obj = std::make_unique<class indirect_object>(make_obj(doc, 1, "source"));
    entries.emplace(1, cross_reference_entry{indirect_reference{1, 0}, std::move(obj)});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(1, std::move(entries));
    cross_reference_section source_section{std::move(subsections)};

    cross_reference_section target{std::deque<cross_reference_subsection>{}};

    auto* source_entry = source_section.find(1);
    REQUIRE(source_entry != nullptr);
    REQUIRE(source_entry->is_resolved());

    auto* target_entry = target.add_entry_from(*source_entry);
    REQUIRE(target_entry != nullptr);

    REQUIRE(target_entry->reference().object_number() == 1);
    REQUIRE(target_entry->reference().generation() == 0);
    REQUIRE(target_entry->is_resolved());
    REQUIRE(*target_entry->indirect_object()->content().as_dictionary()->get_string("Data") ==
            "source");

    source_entry->indirect_object()->content().as_dictionary()->set(
        "Data", object{string_object{"modified"}});
    REQUIRE(*target_entry->indirect_object()->content().as_dictionary()->get_string("Data") ==
            "source");
}

TEST_CASE("add_entry_from copies an unresolved entry", "[cross_reference_section][add_entry_from]")
{
    cross_reference_section source{std::deque<cross_reference_subsection>{}};
    source.add_entry(cross_reference_entry{indirect_reference{3, 0}, 5000, true});

    cross_reference_section target{std::deque<cross_reference_subsection>{}};

    auto* source_entry = source.find(3);
    REQUIRE(source_entry != nullptr);
    REQUIRE_FALSE(source_entry->is_resolved());
    REQUIRE(source_entry->offset() == 5000);

    auto* target_entry = target.add_entry_from(*source_entry);
    REQUIRE(target_entry != nullptr);
    REQUIRE_FALSE(target_entry->is_resolved());
    REQUIRE(target_entry->offset() == 5000);
    REQUIRE(target_entry->in_use());
}

TEST_CASE("push_revision creates revision_history with object 0",
          "[revision_manager][push_revision]")
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

    REQUIRE(manager.all().size() == 1);
    REQUIRE(manager.current().section().size() == 1);

    auto* obj0 = manager.current().section().find(0);
    REQUIRE(obj0 != nullptr);
    REQUIRE_FALSE(obj0->in_use());
    REQUIRE(obj0->reference().generation() == 65535);
    REQUIRE_FALSE(manager.current().section().startxref_offset().has_value());
}

TEST_CASE("create_new_revision creates section and trailer with /Prev",
          "[document][create_new_revision]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(1, cross_reference_entry{indirect_reference{1, 0}, 100, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(1, std::move(entries));

    auto existing_section = cross_reference_section{std::move(subsections), 42};
    trailer existing_trailer{dictionary_object{}};

    std::deque<revision> revisions;
    revisions.emplace_back(std::move(existing_section), std::move(existing_trailer));

    revision_manager manager{std::move(revisions)};

    REQUIRE(manager.all().size() == 1);
    REQUIRE(manager.all()[0].section().startxref_offset().has_value());
    REQUIRE(*manager.all()[0].section().startxref_offset() == 42);
}

TEST_CASE("create_new_revision on document creates section and trailer",
          "[document][create_new_revision]")
{
    document doc{nullptr, nullptr};

    {
        auto& section = doc.cross_reference_table().active_section();
        auto obj = std::make_unique<class indirect_object>(make_obj(doc, 5, "test"));
        section.add_entry(cross_reference_entry{indirect_reference{5, 0}, std::move(obj)});
    }

    auto& new_rev = doc.create_new_revision();

    REQUIRE(doc.revisions().all().size() == 2);
    REQUIRE(new_rev.section().size() == 1);
    auto* obj0 = new_rev.section().find(0);
    REQUIRE(obj0 != nullptr);
    REQUIRE_FALSE(obj0->in_use());

    REQUIRE(doc.trailer().size() == 2);
    REQUIRE(doc.trailer().active_trailer().dictionary().contains("Size"));
    REQUIRE(doc.trailer().active_trailer().dictionary().get_number("Size")->as_integer() >= 6);
    REQUIRE_FALSE(doc.trailer().active_trailer().dictionary().contains("Prev"));
}

TEST_CASE("create_new_revision + add_entry_from for incremental setup",
          "[document][create_new_revision][add_entry_from]")
{
    document doc{nullptr, nullptr};

    {
        auto& section = doc.cross_reference_table().active_section();
        auto obj = std::make_unique<class indirect_object>(make_obj(doc, 42, "original"));
        section.add_entry(cross_reference_entry{indirect_reference{42, 0}, std::move(obj)});
    }

    auto& new_rev = doc.create_new_revision();

    auto* old_entry = doc.cross_reference_table().find(42);
    REQUIRE(old_entry != nullptr);

    auto* copied = new_rev.section().add_entry_from(*old_entry);
    REQUIRE(copied != nullptr);
    REQUIRE(copied->reference().object_number() == 42);
    REQUIRE(copied != nullptr);
    REQUIRE(copied->is_resolved());
    REQUIRE(*copied->indirect_object()->content().as_dictionary()->get_string("Data") ==
            "original");

    old_entry->indirect_object()->content().as_dictionary()->set("Data",
                                                                 object{string_object{"modified"}});
    REQUIRE(*copied->indirect_object()->content().as_dictionary()->get_string("Data") ==
            "original");
}
} // namespace ripper::pdf::core
