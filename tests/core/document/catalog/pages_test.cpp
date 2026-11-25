#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_size.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

namespace ripper::pdf::core
{
namespace
{
geometry::rect media_box_of(page& target)
{
    auto* dict = target.obj().content().as_dictionary();
    REQUIRE(dict != nullptr);

    auto* box = dict->get_array("MediaBox");
    REQUIRE(box != nullptr);
    REQUIRE(box->size() == 4);

    return geometry::rect{(*box)[0].as_number()->as_real(), (*box)[1].as_number()->as_real(),
                          (*box)[2].as_number()->as_real(), (*box)[3].as_number()->as_real()};
}
} // namespace

TEST_CASE("page_size maps to media boxes", "[pages]")
{
    REQUIRE(media_box(page_size::a4) == geometry::rect{0.0, 0.0, 595.0, 842.0});
    REQUIRE(media_box(page_size::letter) == geometry::rect{0.0, 0.0, 612.0, 792.0});
    REQUIRE(media_box(page_size::a4, page_orientation::landscape) ==
            geometry::rect{0.0, 0.0, 842.0, 595.0});
}

TEST_CASE("pages append, prepend and insert in order", "[pages]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_pages_order_test.pdf"};

    auto doc = document::create(output.path());
    auto pages = doc.catalog().pages();

    (void)pages.append_page(page_size::letter);
    (void)pages.prepend_page(page_size::a4);
    (void)pages.insert_page(1, page_size::a3);

    REQUIRE(pages.count() == 3);
    REQUIRE_THROWS_AS(pages.insert_page(4), logic_exception);

    auto first = pages.page(0);
    auto second = pages.page(1);
    auto third = pages.page(2);

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());

    REQUIRE(media_box_of(*first) == media_box(page_size::a4));
    REQUIRE(media_box_of(*second) == media_box(page_size::a3));
    REQUIRE(media_box_of(*third) == media_box(page_size::letter));
}

TEST_CASE("append_page accepts a custom media box", "[pages]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_pages_custom_box_test.pdf"};

    auto doc = document::create(output.path());
    auto pages = doc.catalog().pages();

    auto created = pages.append_page(geometry::rect{100.0, 200.0, -50.0, -100.0});

    REQUIRE(media_box_of(created) == geometry::rect{50.0, 100.0, 50.0, 100.0});
}

TEST_CASE("page index and remove", "[pages]")
{
    test_fixture::scoped_temp_file output{"pdf_ripper_core_pages_remove_test.pdf"};

    auto doc = document::create(output.path());
    auto pages = doc.catalog().pages();

    (void)pages.append_page(page_size::letter);
    (void)pages.append_page(page_size::a4);
    (void)pages.append_page(page_size::a3);

    auto middle = pages.page(1);
    REQUIRE(middle.has_value());
    REQUIRE(middle->index() == 1U);

    middle->remove();

    REQUIRE(pages.count() == 2);

    auto remaining = pages.page(1);
    REQUIRE(remaining.has_value());
    REQUIRE(media_box_of(*remaining) == media_box(page_size::a3));

    REQUIRE_NOTHROW(doc.save());
    REQUIRE(std::filesystem::exists(output.path()));

    auto reopened = document::open(output.path());
    REQUIRE(reopened.catalog().pages().count() == 2);
}
} // namespace ripper::pdf::core
