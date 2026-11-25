#include "ripper/io/core/reader/file_reader.hpp"
#include "ripper/io/core/writer/file_writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

namespace ripper::pdf::core
{
TEST_CASE("document reads a fixture PDF end-to-end", "[document][e2e][read]")
{
    const auto fixture = test_fixture::fixture_pdf_path();

    REQUIRE(std::filesystem::exists(fixture));

    auto doc = document::open(fixture);

    REQUIRE(doc.has_reader());
    REQUIRE(doc.has_parser());
    REQUIRE_FALSE(doc.has_writer());
    REQUIRE_FALSE(doc.has_serializer());

    const auto version = std::string{doc.header().version()};
    REQUIRE_FALSE(version.empty());
    REQUIRE(version.starts_with("2."));

    auto& xref = doc.cross_reference_table();
    REQUIRE(xref.size() > 0);

    auto compiled_trailer = doc.trailer().compiled();
    REQUIRE(compiled_trailer.root().has_value());

    auto catalog = doc.catalog();
    auto pages = catalog.pages();

    REQUIRE(pages.count() > 0);

    std::uint64_t iterated_pages = 0;
    pages.each([&iterated_pages](page&) { ++iterated_pages; });

    REQUIRE(iterated_pages == pages.count());
}

TEST_CASE("document writes and re-opens a created PDF", "[document][e2e][write]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_document_test_output.pdf"};

    auto writer_doc = document::create(output.path());

    REQUIRE_FALSE(writer_doc.has_reader());
    REQUIRE_FALSE(writer_doc.has_parser());
    REQUIRE(writer_doc.has_writer());
    REQUIRE(writer_doc.has_serializer());

    auto pages = writer_doc.catalog().pages();
    (void)pages.append_page();
    (void)pages.append_page();
    (void)pages.append_page();

    REQUIRE_NOTHROW(writer_doc.save());
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back_doc = document::open(output.path());
    REQUIRE(read_back_doc.catalog().pages().count() == 3);
}

TEST_CASE("append_page updates /Count on the pages node", "[document][e2e][write]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_document_add_count_test.pdf"};

    auto writer_doc = document::create(output.path());

    auto pages = writer_doc.catalog().pages();
    REQUIRE(pages.count() == 0);

    (void)pages.append_page();
    REQUIRE(pages.count() == 1);

    (void)pages.append_page();
    REQUIRE(pages.count() == 2);

    (void)pages.append_page();
    REQUIRE(pages.count() == 3);

    REQUIRE_NOTHROW(writer_doc.save());
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back_doc = document::open(output.path());
    REQUIRE(read_back_doc.catalog().pages().count() == 3);
}

TEST_CASE("document edits an already existing PDF", "[document][e2e][write]")
{
    test_fixture::scoped_temp_file output_file{"pdf_ripper_core_document_edit_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    const auto output_path = output_file.path();

    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output_path);
    document doc{std::move(reader), std::move(writer)};

    REQUIRE(doc.has_reader());
    REQUIRE(doc.has_writer());

    auto pages = doc.catalog().pages();

    const auto initial_count = pages.count();
    REQUIRE(initial_count > 0);

    pages.remove(2);
    REQUIRE(pages.count() == initial_count - 1);

    REQUIRE_NOTHROW(doc.save());
    REQUIRE(std::filesystem::exists(output_path));

    auto read_back = document::open(output_path);
    REQUIRE(read_back.catalog().pages().count() == initial_count - 1);
}
TEST_CASE("resolve_object_to_active_revision clones object from old section",
          "[document][revision]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_revision_clone_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    auto pages_ref = doc.catalog().root_pages_indirect_reference();
    auto* original_entry = doc.cross_reference_table().find(pages_ref);
    REQUIRE(original_entry != nullptr);

    // Create a new revision
    doc.create_new_revision();

    // Resolve the pages object to the active revision
    auto& cloned_obj = doc.resolve_object_to_active_revision(pages_ref);

    // Verify the cloned object has the same reference
    REQUIRE(cloned_obj.identity().reference() == pages_ref);

    // Verify the original and cloned are different objects (different addresses)
    auto* original_obj = original_entry->indirect_object();
    REQUIRE(original_obj != nullptr);
    REQUIRE(original_obj != &cloned_obj);

    // Verify the cloned object is in the active section
    auto& active = doc.cross_reference_table().active_section();
    auto* cloned_entry = active.find(pages_ref);
    REQUIRE(cloned_entry != nullptr);
    REQUIRE(cloned_entry->indirect_object() == &cloned_obj);
}

TEST_CASE("resolve_object_to_active_revision returns existing when already in active section",
          "[document][revision]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_revision_already_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    // Create a new revision, then clone an object into it
    doc.create_new_revision();
    auto pages_ref = doc.catalog().root_pages_indirect_reference();
    auto& first_clone = doc.resolve_object_to_active_revision(pages_ref);

    // Resolve again — should return the same object (no second clone)
    auto& second_clone = doc.resolve_object_to_active_revision(pages_ref);
    REQUIRE(&first_clone == &second_clone);
}

TEST_CASE("resolve_object_to_active_revision throws for non-existent object",
          "[document][revision][error]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_revision_notfound_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    doc.create_new_revision();

    indirect_reference nonexistent{99999, 0};
    REQUIRE_THROWS_AS(doc.resolve_object_to_active_revision(nonexistent), logic_exception);
}

TEST_CASE("rebind_to_active_revision updates view to point to active revision clone",
          "[document][revision]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_rebind_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    auto pages = doc.catalog().pages();
    auto* original_obj = &pages.obj();
    auto pages_ref = pages.obj().identity().reference();

    // Create a new revision
    doc.create_new_revision();

    // Call rebind_to_active_revision
    pages.rebind_to_active_revision();

    // The view should now point to a different object (the clone in the active section)
    auto* new_obj = &pages.obj();
    REQUIRE(new_obj != original_obj);

    // The new object should be in the active section
    auto& active = doc.cross_reference_table().active_section();
    auto* entry = active.find(pages_ref);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->indirect_object() == new_obj);
}
} // namespace ripper::pdf::core
