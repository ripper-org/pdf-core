#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace ripper::pdf::core
{

indirect_object make_indirect_object(document& doc, std::uint32_t obj_num, std::string value)
{
    dictionary_object dict;
    dict.set("Value", object{std::move(value)});
    return indirect_object{object_identity{&doc, indirect_reference{obj_num, 0}},
                           object{std::move(dict)}};
}

TEST_CASE("object clone creates independent deep copy of dictionary", "[object][copy][clone]")
{
    dictionary_object dict;
    dict.set("Name", object{name_object{"original"}});
    dict.set("Count", object{std::int64_t{42}});

    object original{std::move(dict)};
    object copy = original.clone();

    REQUIRE(copy.as_dictionary()->get_name("Name")->value == "original");
    REQUIRE(copy.as_dictionary()->get_number("Count")->as_integer() == 42);

    // Modify original — copy must be unaffected
    original.as_dictionary()->set("Name", object{name_object{"modified"}});
    original.as_dictionary()->set("Count", object{std::int64_t{0}});

    REQUIRE(copy.as_dictionary()->get_name("Name")->value == "original");
    REQUIRE(copy.as_dictionary()->get_number("Count")->as_integer() == 42);
}

TEST_CASE("object clone creates independent deep copy of nested dictionary",
          "[object][copy][clone]")
{
    dictionary_object inner;
    inner.set("InnerKey", object{string_object{"inner"}});

    dictionary_object outer;
    outer.set("Nested", object{std::move(inner)});

    object original{std::move(outer)};
    object copy = original.clone();

    REQUIRE(*copy.as_dictionary()->get_dictionary("Nested")->get_string("InnerKey") == "inner");

    original.as_dictionary()->get_dictionary("Nested")->set("InnerKey",
                                                            object{string_object{"changed"}});
    REQUIRE(*copy.as_dictionary()->get_dictionary("Nested")->get_string("InnerKey") == "inner");
}

TEST_CASE("object clone creates independent deep copy of stream", "[object][copy][clone]")
{
    std::vector<std::byte> data = {std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
    stream str{data};
    dictionary_object dict;
    dict.set("Length", object{std::int64_t{3}});

    stream_object os{std::move(dict), std::move(str)};
    object original{std::move(os)};
    object copy = original.clone();

    REQUIRE(copy.as_stream()->stream().size() == 3);
    REQUIRE(copy.as_stream()->stream().data()[0] == std::byte{'a'});
}

TEST_CASE("object clone creates independent deep copy of array with dictionaries",
          "[object][copy][clone]")
{
    dictionary_object item;
    item.set("Id", object{std::int64_t{1}});

    array_object arr;
    arr.push_back(object{std::move(item)});

    object original{std::move(arr)};
    object copy = original.clone();

    REQUIRE(copy.as_array()->at(0).as_dictionary()->get_number("Id")->as_integer() == 1);

    original.as_array()->at(0).as_dictionary()->set("Id", object{std::int64_t{99}});
    REQUIRE(copy.as_array()->at(0).as_dictionary()->get_number("Id")->as_integer() == 1);
}

TEST_CASE("indirect_object clone creates independent deep copy", "[indirect_object][copy][clone]")
{
    document doc{nullptr, nullptr};

    auto original = make_indirect_object(doc, 1, "hello");

    auto cloned = original.clone();

    REQUIRE(cloned.identity().reference().object_number() == 1);
    REQUIRE(cloned.identity().reference().generation() == 0);
    REQUIRE(*cloned.content().as_dictionary()->get_string("Value") == "hello");

    original.content().as_dictionary()->set("Value", object{string_object{"modified"}});
    REQUIRE(*cloned.content().as_dictionary()->get_string("Value") == "hello");
}

TEST_CASE("cross_reference_entry copy deep-clones resolved indirect object",
          "[cross_reference_entry][copy]")
{
    document doc{nullptr, nullptr};

    auto obj = std::make_unique<class indirect_object>(make_indirect_object(doc, 5, "entry"));

    cross_reference_entry original{indirect_reference{3, 1}, std::move(obj)};
    REQUIRE(original.is_resolved());
    REQUIRE(original.in_use());
    REQUIRE(*original.indirect_object()->content().as_dictionary()->get_string("Value") == "entry");

    cross_reference_entry copy{original};

    REQUIRE(copy.reference().object_number() == 3);
    REQUIRE(copy.reference().generation() == 1);
    REQUIRE(copy.is_resolved());
    REQUIRE(copy.in_use());
    REQUIRE(*copy.indirect_object()->content().as_dictionary()->get_string("Value") == "entry");

    // Modify original — copy must be untouched
    original.indirect_object()->content().as_dictionary()->set("Value",
                                                               object{string_object{"modified"}});
    REQUIRE(*copy.indirect_object()->content().as_dictionary()->get_string("Value") == "entry");
}

TEST_CASE("cross_reference_entry copy handles unresolved entries", "[cross_reference_entry][copy]")
{
    cross_reference_entry original{indirect_reference{7, 0}, 12345, true};
    REQUIRE_FALSE(original.is_resolved());

    cross_reference_entry copy{original};

    REQUIRE(copy.reference().object_number() == 7);
    REQUIRE(copy.reference().generation() == 0);
    REQUIRE_FALSE(copy.is_resolved());
    REQUIRE(copy.offset() == 12345);
    REQUIRE(copy.in_use());
}

TEST_CASE("cross_reference_entry copy handles pending (reserved) entries",
          "[cross_reference_entry][copy]")
{
    cross_reference_entry original{indirect_reference{9, 0}};
    REQUIRE(original.is_new());
    REQUIRE_FALSE(original.is_resolved());

    cross_reference_entry copy{original};

    REQUIRE(copy.reference().object_number() == 9);
    REQUIRE(copy.is_new());
    REQUIRE_FALSE(copy.is_resolved());
}

TEST_CASE("cross_reference_entry copy handles deleted entries", "[cross_reference_entry][copy]")
{
    document doc{nullptr, nullptr};

    auto obj = std::make_unique<class indirect_object>(make_indirect_object(doc, 4, "deletable"));

    cross_reference_entry original{indirect_reference{4, 0}, std::move(obj)};
    original.mark_deleted();
    REQUIRE_FALSE(original.in_use());
    REQUIRE(original.is_resolved());

    cross_reference_entry copy{original};

    REQUIRE(copy.reference().object_number() == 4);
    REQUIRE_FALSE(copy.in_use());
    REQUIRE(copy.is_resolved());
}

TEST_CASE("cross_reference_section copy creates independent section",
          "[cross_reference_section][copy]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(1, cross_reference_entry{indirect_reference{1, 0}, 100, true});
    entries.emplace(2, cross_reference_entry{indirect_reference{2, 0}, 200, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(1, std::move(entries));

    cross_reference_section original{std::move(subsections)};
    REQUIRE(original.size() == 2);

    cross_reference_section copy{original};

    REQUIRE(copy.size() == 2);
    REQUIRE(copy.find(1) != nullptr);
    REQUIRE(copy.find(2) != nullptr);
    REQUIRE(copy.find(1)->in_use());
    REQUIRE(copy.find(2)->offset() == 200);
    REQUIRE_FALSE(copy.startxref_offset().has_value());
}

TEST_CASE("cross_reference_section copy assignment creates independent section",
          "[cross_reference_section][copy][assign]")
{
    cross_reference_subsection::entry_map entries;
    entries.emplace(5, cross_reference_entry{indirect_reference{5, 0}, 500, true});

    std::deque<cross_reference_subsection> subsections;
    subsections.emplace_back(5, std::move(entries));

    cross_reference_section original{std::move(subsections)};

    cross_reference_subsection::entry_map empty_entries;
    std::deque<cross_reference_subsection> empty_subsections;
    empty_subsections.emplace_back(0, std::move(empty_entries));
    cross_reference_section copy{std::move(empty_subsections)};

    copy = original;

    REQUIRE(copy.size() == 1);
    REQUIRE(copy.find(5) != nullptr);
    REQUIRE(copy.find(5)->reference().object_number() == 5);
}
} // namespace ripper::pdf::core
