#include "ripper/pdf/core/document/catalog/pages/page_tree_node.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/catalog.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace ripper::pdf::core
{
page_tree_node::page_tree_node(indirect_object& obj) noexcept : indirect_object_view(obj) {}

bool page_tree_node::is_leaf() const
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return false;

    const auto* type = d->get_name("Type");
    return type != nullptr && type->value == "Page";
}

bool page_tree_node::is_root() const
{
    const auto root_ref = obj().identity().owner().catalog().root_pages_indirect_reference();
    return root_ref == obj().identity().reference();
}

std::optional<page_tree_node> page_tree_node::parent()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return std::nullopt;

    const auto* parent_ref = d->get_indirect_reference("Parent");
    if (parent_ref == nullptr)
        return std::nullopt;

    auto* resolved = obj().identity().owner().resolve_object(*parent_ref);
    if (resolved == nullptr)
        return std::nullopt;

    return page_tree_node{*resolved};
}

std::vector<page_tree_node> page_tree_node::children()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return {};

    const auto* kids = d->get_array("Kids");
    if (kids == nullptr)
        return {};

    std::vector<page_tree_node> result;
    result.reserve(kids->size());

    for (const auto& kid_obj : *kids)
    {
        const auto* ref = kid_obj.as_indirect_reference();
        if (ref == nullptr)
            throw logic_exception{"Kid entry is not an indirect reference"};

        auto* resolved = obj().identity().owner().resolve_object(*ref);
        if (resolved == nullptr)
            throw logic_exception{"Failed to resolve kid reference"};

        result.emplace_back(*resolved);
    }

    return result;
}

void page_tree_node::remove_child(const indirect_reference& ref)
{
    rebind_to_active_revision();

    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page tree node content is not a dictionary"};

    auto* kids = d->get_array("Kids");
    if (kids == nullptr)
        throw logic_exception{"Pages node is missing required /Kids entry"};

    auto it = std::find_if(kids->begin(), kids->end(),
                           [&](const object& o)
                           {
                               const auto* r = o.as_indirect_reference();
                               return r != nullptr && *r == ref;
                           });

    if (it != kids->end())
        kids->erase(it);
}

void page_tree_node::insert_child(std::size_t child_index, const indirect_reference& ref)
{
    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page tree node content is not a dictionary"};

    if (d->get_array("Kids") == nullptr)
        d->set("Kids", object{array_object{}});

    auto* kids = d->get_array("Kids");
    auto& items = kids->items();
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(child_index), object{ref});
}

void page_tree_node::bump_count(std::int64_t delta)
{
    std::unordered_set<indirect_reference> visited;

    page_tree_node node = *this;
    while (true)
    {
        const auto node_ref = node.obj().identity().reference();
        if (!visited.insert(node_ref).second)
            throw logic_exception{"Cyclic /Parent chain detected while climbing the page tree"};

        node.rebind_to_active_revision();

        auto* d = node.obj().content().as_dictionary();
        if (d == nullptr)
            break;

        auto* count = d->get_number("Count");
        if (count == nullptr)
            break;

        if (delta < 0 && count->as_integer() <= 0)
            break;

        d->set("Count", object{count->as_integer() + delta});

        if (node.is_root())
            break;

        auto up = node.parent();
        if (!up)
            break;

        node = *up;
    }
}

std::optional<class pages> page_tree_node::as_pages()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return std::nullopt;

    const auto* type = d->get_name("Type");
    if (type == nullptr || type->value != "Pages")
        return std::nullopt;

    return pages{obj()};
}

std::optional<class page> page_tree_node::as_page()
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        return std::nullopt;

    const auto* type = d->get_name("Type");
    if (type == nullptr || type->value != "Page")
        return std::nullopt;

    return page{obj()};
}

std::uint64_t page_tree_node::subtree_count() const
{
    const auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page tree node content is not a dictionary"};

    const auto* type = d->get_name("Type");
    if (type != nullptr && type->value == "Page")
        return 1;

    const auto* count = d->get_number("Count");
    if (count == nullptr)
        throw logic_exception{"Pages node is missing required /Count entry"};

    return static_cast<std::uint64_t>(count->as_integer());
}
} // namespace ripper::pdf::core
