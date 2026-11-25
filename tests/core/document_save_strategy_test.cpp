#include "ripper/io/core/reader/file_reader.hpp"
#include "ripper/io/core/writer/file_writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document_save_strategy/consolidate_document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/dump_document_save_strategy.hpp"
#include "ripper/pdf/core/document_save_strategy/incremental_document_save_strategy.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <memory>
#include <vector>

namespace ripper::pdf::core
{
TEST_CASE("consolidate save strategy writes and re-opens a created PDF",
          "[document][e2e][save][consolidate]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_consolidate_save_test.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();
    (void)pages.append_page();

    consolidate_document_save_strategy strategy;
    REQUIRE_NOTHROW(strategy.save(doc));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 2);
}

TEST_CASE("dump save strategy writes and re-opens a created PDF", "[document][e2e][save][dump]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_dump_save_test.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();
    (void)pages.append_page();

    dump_document_save_strategy strategy;
    REQUIRE_NOTHROW(strategy.save(doc));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 2);
}

TEST_CASE("save with save_strategy_type enum selects built-in strategy",
          "[document][e2e][save][enum]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_enum_save_test.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();

    REQUIRE_NOTHROW(doc.save(save_strategy_type::consolidate));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 1);
}

TEST_CASE("set_save_strategy injects a custom strategy", "[document][e2e][save][strategy]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_custom_strategy_test.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();
    (void)pages.append_page();
    (void)pages.append_page();

    doc.set_save_strategy(std::make_unique<consolidate_document_save_strategy>());
    REQUIRE_NOTHROW(doc.save());
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("set_save_strategy with nullptr resets to default", "[document][e2e][save][strategy]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_reset_strategy_test.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();

    doc.set_save_strategy(std::make_unique<consolidate_document_save_strategy>());
    doc.set_save_strategy(nullptr);
    REQUIRE_NOTHROW(doc.save());

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 1);
}

TEST_CASE("save with enum ignores injected strategy", "[document][e2e][save][enum][strategy]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_enum_ignores_strategy.pdf"};

    auto doc = document::create(output.path());

    auto pages = doc.catalog().pages();
    (void)pages.append_page();
    (void)pages.append_page();

    doc.set_save_strategy(std::make_unique<dump_document_save_strategy>());
    REQUIRE_NOTHROW(doc.save(save_strategy_type::consolidate));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 2);
}

TEST_CASE("dump save preserves existing PDF", "[document][e2e][save][dump]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_dump_edit_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    dump_document_save_strategy strategy;
    REQUIRE_NOTHROW(strategy.save(doc));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("consolidate save preserves existing PDF", "[document][e2e][save][consolidate]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_consolidate_edit_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    consolidate_document_save_strategy strategy;
    REQUIRE_NOTHROW(strategy.save(doc));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("dump save throws without writer", "[document][save][error]")
{
    auto doc = document::open(test_fixture::fixture_pdf_path());

    dump_document_save_strategy strategy;
    REQUIRE_THROWS_AS(strategy.save(doc), logic_exception);
}

TEST_CASE("consolidate save throws without writer", "[document][save][error]")
{
    auto doc = document::open(test_fixture::fixture_pdf_path());

    consolidate_document_save_strategy strategy;
    REQUIRE_THROWS_AS(strategy.save(doc), logic_exception);
}

TEST_CASE("incremental save preserves existing PDF", "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_preserve_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Re-open and verify the original content is intact.
    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("incremental save with new revision produces larger file",
          "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_size_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    auto input_size = std::filesystem::file_size(input_path);

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    static_cast<void>(doc.create_new_revision());

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    auto output_size = std::filesystem::file_size(output.path());
    REQUIRE(output_size > input_size);

    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("incremental save throws without reader", "[document][save][error][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_no_reader_test.pdf"};

    auto doc = document::create(output.path());

    REQUIRE_THROWS_AS(doc.save(save_strategy_type::incremental), logic_exception);
}

TEST_CASE("incremental strategy throws without reader", "[document][save][error][incremental]")
{
    auto doc = document::open(test_fixture::fixture_pdf_path());

    incremental_document_save_strategy strategy;
    REQUIRE_THROWS_AS(strategy.save(doc), logic_exception);
}
TEST_CASE("incremental save with new page preserves original bytes",
          "[document][e2e][save][incremental]")
{
    const auto input_path = test_fixture::fixture_pdf_path();
    auto input_size = std::filesystem::file_size(input_path);

    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_bytes_test.pdf"};

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    static_cast<void>(doc.create_new_revision());

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Read the first input_size bytes of both files and compare.
    std::ifstream in{input_path, std::ios::binary};
    std::ifstream out{output.path(), std::ios::binary};
    REQUIRE(in.is_open());
    REQUIRE(out.is_open());

    std::vector<char> in_data(input_size);
    std::vector<char> out_data(input_size);
    in.read(in_data.data(), static_cast<std::streamsize>(input_size));
    out.read(out_data.data(), static_cast<std::streamsize>(input_size));

    REQUIRE(in_data == out_data);
}

TEST_CASE("incremental save with page tree copy adds a page", "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_page_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    // Create a new revision and copy the page tree entry into it.
    auto& new_rev = doc.create_new_revision();

    auto pages_ref = doc.catalog().root_pages_indirect_reference();
    auto* pages_entry = doc.cross_reference_table().find(pages_ref);
    REQUIRE(pages_entry != nullptr);

    auto* copied_pages = new_rev.section().add_entry_from(*pages_entry);
    REQUIRE(copied_pages != nullptr);

    // Now add a page, the lookup resolves the page tree from the new section.
    auto pages = doc.catalog().pages();
    (void)pages.append_page();

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Re-open and verify 4 pages.
    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 4);
}

TEST_CASE("incremental save with rebind_to_active_revision adds a page without manual cloning",
          "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_rebind_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    // Create a new revision and rely on append_page's internal rebind_to_active_revision
    doc.create_new_revision();

    auto pages = doc.catalog().pages();
    REQUIRE(pages.count() == 3);

    (void)pages.append_page();

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Re-open and verify 4 pages
    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 4);
}

TEST_CASE("incremental save with rebind_to_active_revision deletes a page without manual cloning",
          "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_rebind_del_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    // Create a new revision and rely on remove's internal rebind_to_active_revision
    doc.create_new_revision();

    auto pages = doc.catalog().pages();
    REQUIRE(pages.count() == 3);

    pages.remove(0);
    REQUIRE(pages.count() == 2);

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Re-open and verify 2 pages
    auto read_back = document::open(output.path());
    REQUIRE(read_back.catalog().pages().count() == 2);
}

TEST_CASE("incremental save with multiple new sections", "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_multi_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    // Push two new sections.
    static_cast<void>(doc.create_new_revision());
    static_cast<void>(doc.create_new_revision());

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    // Re-open. Should have 3 sections (1 original + 2 new).
    auto read_back = document::open(output.path());
    REQUIRE(read_back.revisions().all().size() == 3);
    REQUIRE(read_back.catalog().pages().count() == 3);
}

TEST_CASE("incremental save preserves first /ID element and generates new second",
          "[document][e2e][save][incremental]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_inc_id_test.pdf"};

    const auto input_path = test_fixture::fixture_pdf_path();
    REQUIRE(std::filesystem::exists(input_path));

    auto reader = std::make_unique<ripper::io::core::file_reader>(input_path);
    auto writer = std::make_unique<ripper::io::core::file_writer>(output.path());
    document doc{std::move(reader), std::move(writer)};

    {
        array_object id_arr;
        id_arr.push_back(object{string_object{"original-id-hash"}});
        auto& first_trailer = doc.revisions().all()[0].trailer();
        first_trailer.dictionary().set("ID", object{std::move(id_arr)});
    }

    doc.create_new_revision();

    REQUIRE_NOTHROW(doc.save(save_strategy_type::incremental));
    REQUIRE(std::filesystem::exists(output.path()));

    auto read_back = document::open(output.path());
    auto id = read_back.trailer().compiled().id();
    REQUIRE(id.has_value());
    REQUIRE(id->original() == "original-id-hash");
    REQUIRE(id->current().has_value());
}
} // namespace ripper::pdf::core
