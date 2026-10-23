#include "ripper/io/core/reader/file_reader.hpp"
#include "ripper/io/core/writer/file_writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <utility>

namespace ripper::pdf::core
{
TEST_CASE("moving a document before parsing keeps it usable", "[document][move]")
{
    auto doc = document::open(test_fixture::fixture_pdf_path());

    document moved{std::move(doc)};

    REQUIRE_FALSE(doc.has_reader());
    REQUIRE_FALSE(doc.has_parser());
    REQUIRE(moved.has_reader());
    REQUIRE(moved.has_parser());
    REQUIRE(std::string{moved.header().version()}.starts_with("2."));
    REQUIRE(moved.catalog().pages().count() > 0);
}

TEST_CASE("moving a document after parsing rebinds owners and preserves views", "[document][move]")
{
    auto doc = document::open(test_fixture::fixture_pdf_path());

    // Force lazy parser subcomponents into existence before the move.
    (void)doc.header();
    (void)doc.cross_reference_table();
    auto pages = doc.catalog().pages();
    const auto page_count = pages.count();
    auto* pages_object = &pages.obj();

    document moved{std::move(doc)};

    REQUIRE(pages.count() == page_count);
    REQUIRE(&pages.obj() == pages_object);
    REQUIRE(&pages.obj().identity().owner() == &moved);
    REQUIRE(moved.catalog().pages().count() == page_count);
}

TEST_CASE("moving a document preserves serializer configuration", "[document][move][save]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_document_move_save_test.pdf"};

    auto doc =
        document{std::make_unique<ripper::io::core::file_reader>(test_fixture::fixture_pdf_path()),
                 std::make_unique<ripper::io::core::file_writer>(output.path())};

    const auto page_count = doc.catalog().pages().count();

    document moved{std::move(doc)};
    REQUIRE_NOTHROW(moved.save());

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == page_count);
}

TEST_CASE("move assignment transfers state and rebinds owners", "[document][move]")
{
    auto source = document::open(test_fixture::fixture_pdf_path());
    auto pages = source.catalog().pages();
    const auto page_count = pages.count();
    auto* pages_object = &pages.obj();

    document target{nullptr, nullptr};
    target = std::move(source);

    REQUIRE_FALSE(source.has_reader());
    REQUIRE(target.has_reader());
    REQUIRE(&pages.obj() == pages_object);
    REQUIRE(&pages.obj().identity().owner() == &target);
    REQUIRE(target.catalog().pages().count() == page_count);
}

TEST_CASE("copying a document deep-copies state and rebinds owners", "[document][copy]")
{
    auto source = document::open(test_fixture::fixture_pdf_path());
    const auto page_count = source.catalog().pages().count();

    document copy{source};

    REQUIRE(copy.has_reader());
    REQUIRE(copy.has_parser());
    REQUIRE_FALSE(copy.has_writer());
    REQUIRE_FALSE(copy.has_serializer());

    auto copy_pages = copy.catalog().pages();
    REQUIRE(&copy_pages.obj().identity().owner() == &copy);
    REQUIRE(copy_pages.count() == page_count);

    (void)copy_pages.add_page();

    REQUIRE(copy_pages.count() == page_count + 1);
    REQUIRE(source.catalog().pages().count() == page_count);
}

TEST_CASE("copying a document preserves header and limits", "[document][copy]")
{
    auto source = document::open(test_fixture::fixture_pdf_path());

    resource_limits limits;
    limits.max_parse_depth = 123;
    source.set_resource_limits(limits);

    (void)source.header();

    document copy{source};

    REQUIRE(copy.resource_limits().max_parse_depth == 123);
    REQUIRE(copy.header().version() == source.header().version());
}

TEST_CASE("copying an unparsed document keeps it lazy after the source dies",
          "[document][copy][lazy]")
{
    const auto fixture = test_fixture::fixture_pdf_path();

    document copy{nullptr, nullptr};
    {
        auto source = document::open(fixture);
        copy = source;
    }

    REQUIRE(copy.has_reader());
    REQUIRE(std::string{copy.header().version()}.starts_with("2."));
    REQUIRE(copy.catalog().pages().count() > 0);
}

TEST_CASE("copy assignment replaces the destination state", "[document][copy]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_document_copy_assignment.pdf"};

    auto source = document::open(test_fixture::fixture_pdf_path());
    const auto page_count = source.catalog().pages().count();

    document target = document::create(output.path());
    REQUIRE(target.has_writer());

    target = source;

    REQUIRE(target.has_reader());
    REQUIRE_FALSE(target.has_writer());
    REQUIRE(&target.catalog().pages().obj().identity().owner() == &target);
    REQUIRE(target.catalog().pages().count() == page_count);
}

TEST_CASE("copying a document rebinds objects in every revision", "[document][copy][revision]")
{
    auto source = document::open(test_fixture::fixture_pdf_path());

    const auto pages_ref = source.catalog().root_pages_indirect_reference();
    REQUIRE(source.resolve_object(pages_ref) != nullptr);

    const auto revision_count = source.revisions().size();
    source.create_new_revision();
    (void)source.resolve_object_to_active_revision(pages_ref);

    document copy{source};

    REQUIRE(copy.revisions().size() == revision_count + 1);

    for (auto& revision : copy.revisions().all())
    {
        for (auto& subsection : revision.section().subsections())
        {
            for (auto& entry : subsection.entries())
            {
                auto* object = entry.second.indirect_object();
                if (object != nullptr)
                    REQUIRE(&object->identity().owner() == &copy);
            }
        }
    }

    // The copy owns independent clones; resolving in the copy must not touch
    // the source document.
    REQUIRE(copy.resolve_object(pages_ref) != nullptr);
    REQUIRE(source.resolve_object(pages_ref) != nullptr);
}

TEST_CASE("attaching a writer makes a copied document saveable", "[document][copy][save]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_document_copy_save_test.pdf"};

    auto source = document::open(test_fixture::fixture_pdf_path());
    const auto page_count = source.catalog().pages().count();

    document copy{source};
    copy.set_writer(std::make_unique<ripper::io::core::file_writer>(output.path()));

    REQUIRE(copy.has_writer());
    REQUIRE(copy.has_serializer());

    auto pages = copy.catalog().pages();
    (void)pages.add_page();

    REQUIRE_NOTHROW(copy.save());

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == page_count + 1);
    REQUIRE(source.catalog().pages().count() == page_count);

    copy.set_writer();
    REQUIRE_FALSE(copy.has_writer());
    REQUIRE_FALSE(copy.has_serializer());
}
} // namespace ripper::pdf::core
