#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_size.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_tree_node.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstdint>
#include <optional>

namespace ripper::pdf::core
{
namespace
{
/// Returns this page's parent /Pages node, if it resolves to one.
[[nodiscard]] std::optional<pages> parent_pages(page& target)
{
    auto up = page_tree_node{target.obj()}.parent();
    if (!up)
        return std::nullopt;

    return up->as_pages();
}
} // namespace

page::page(indirect_object& obj) noexcept : object_view(obj) {}

geometry::rect page::media_box()
{
    const auto letter = ripper::pdf::core::media_box(page_size::letter);
    const auto* dict = obj().content().as_dictionary();

    if (const auto* value = obj().identity().owner().resolve_object_deep(
            dict != nullptr ? dict->get("MediaBox") : nullptr);
        value != nullptr && !value->is_null())
        return geometry::rect::from(obj().identity().owner(), *value).value_or(letter);

    if (auto up = parent_pages(*this))
        return up->media_box().value_or(letter);

    return letter;
}

geometry::rect page::crop_box()
{
    const auto* dict = obj().content().as_dictionary();

    if (const auto* value = obj().identity().owner().resolve_object_deep(
            dict != nullptr ? dict->get("CropBox") : nullptr);
        value != nullptr && !value->is_null())
        return geometry::rect::from(obj().identity().owner(), *value).value_or(media_box());

    if (auto up = parent_pages(*this))
        return up->crop_box().value_or(media_box());

    return media_box();
}

page_rotation page::rotation()
{
    const auto* dict = obj().content().as_dictionary();

    if (const auto* value = obj().identity().owner().resolve_object_deep(
            dict != nullptr ? dict->get("Rotate") : nullptr);
        value != nullptr && !value->is_null())
        return page_rotation::from(*value);

    if (auto up = parent_pages(*this))
        return up->rotation().value_or(page_rotation{0});

    return page_rotation{0};
}

double page::width()
{
    const auto box = media_box();
    return rotation().is_quarter_turn() ? box.height : box.width;
}

double page::height()
{
    const auto box = media_box();
    return rotation().is_quarter_turn() ? box.width : box.height;
}

void page::rotate(int degrees)
{
    const page_rotation normalized{degrees};

    rebind_to_active_revision();

    auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page object view object is not a dictionary"};

    dict->set("Rotate", object{static_cast<std::int64_t>(normalized.degrees())});
}

pages page::parent()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page object view object is not a dictionary"};

    const auto* parent_ref = d->get_indirect_reference("Parent");
    if (parent_ref == nullptr)
        throw logic_exception{"Page /Parent is not an indirect referece"};

    auto* resolved = obj().identity().owner().resolve_object(*parent_ref);
    if (resolved == nullptr)
        throw logic_exception{"Could not resolve Page /Parent indirect refernce"};

    const auto* parent_dict = resolved->content().as_dictionary();
    if (parent_dict == nullptr)
        throw logic_exception{"Page /Parent is not a dictionary"};

    const auto* type = parent_dict->get_name("Type");
    if (type == nullptr || type->value != "Pages")
        throw logic_exception{"Page /Parent object does not have /Type /Pages"};

    return pages{*resolved};
}

std::uint64_t page::index()
{
    page_tree_node node{obj()};
    auto parent = node.parent();
    if (!parent)
        throw logic_exception{"Page has no /Parent and no index"};

    std::uint64_t position = 0;

    for (auto kid : parent->children())
    {
        if (kid.obj().identity().reference() == obj().identity().reference())
            return position;

        position += kid.is_leaf() ? 1 : kid.subtree_count();
    }

    throw logic_exception{"Page is not present in its parent's /Kids"};
}

void page::remove()
{
    auto parent_pages = parent();
    parent_pages.remove(index());
}

page_contents page::content()
{
    auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Page content is not a dictionary"};

    auto& doc = obj().identity().owner();

    // A fresh page has no /Contents entry yet; allocate one on first access so
    // callers can always write to the page's content.
    if (dict->get("Contents") == nullptr)
        return object_manager::create_page_contents(doc, *this);

    return object_manager::parse_page_contents(*this);
}

canvas page::canvas(canvas_mode mode)
{
    rebind_to_active_revision();

    auto& doc = obj().identity().owner();

    page_contents contents{obj()};

    // Allocate before touching /Contents so a failed commit leaves the page intact.
    auto stream = object_manager::create_content_stream(doc);

    switch (mode)
    {
        case canvas_mode::append:
            contents.append(stream);
            break;

        case canvas_mode::prepend:
            contents.insert(0, stream);
            break;

        case canvas_mode::replace:
            contents.clear();
            contents.append(stream);
            break;
    }

    auto* stream_obj = stream.stream();
    if (stream_obj == nullptr)
        throw logic_exception{"Newly created content stream is not a stream"};

    return ripper::pdf::core::canvas{*stream_obj};
}
} // namespace ripper::pdf::core
