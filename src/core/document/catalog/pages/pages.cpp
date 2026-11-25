#include "ripper/pdf/core/document/catalog/pages/pages.hpp"

#include "core/document/catalog/pages/page_attribute.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_tree_node.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

namespace ripper::pdf::core
{
namespace
{
/// Inserts `ref` at `child_index` in the node's /Kids, creating /Kids if absent.
void insert_kid(pages& owner, std::size_t child_index, const indirect_reference& ref)
{
    auto* d = owner.obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Pages content is not a dictionary"};

    if (d->get_array("Kids") == nullptr)
        d->set("Kids", object{array_object{}});

    auto* kids = d->get_array("Kids");
    auto& items = kids->items();
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(child_index), object{ref});
}

/// Adds `delta` to /Count on `node` and every ancestor, stopping at the root.
void bump_counts(page_tree_node node, std::int64_t delta)
{
    std::unordered_set<indirect_reference> visited;

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
} // namespace

pages::pages(indirect_object& obj) noexcept : object_view(obj) {}

std::uint64_t pages::count() const
{
    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Pages content is not a dictionary"};

    auto count = d->get_number("Count");
    if (count == nullptr)
        throw logic_exception{"Pages indirect_object is missing required /Count entry"};

    return static_cast<std::uint64_t>(count->as_integer());
}

std::optional<pages> pages::parent()
{
    auto up = page_tree_node{obj()}.parent();
    if (!up)
        return std::nullopt;

    return up->as_pages();
}

const object* pages::inherited(std::string_view key)
{
    auto* current = &obj();
    std::unordered_set<indirect_reference> visited;

    while (true)
    {
        if (!visited.insert(current->identity().reference()).second)
            throw logic_exception{"Cyclic /Parent chain detected while resolving inherited page "
                                  "attribute"};

        const auto* dict = current->content().as_dictionary();
        if (dict == nullptr)
        {
            if (current == &obj())
                throw logic_exception{"Pages object view object is not a dictionary"};

            return nullptr;
        }

        if (const auto* value = dict->get(std::string{key}); value != nullptr)
        {
            if (const auto* ref = value->as_indirect_reference())
            {
                auto* resolved = current->identity().owner().resolve_object(*ref);
                if (resolved != nullptr && !resolved->content().is_null())
                    return &resolved->content();
            }
            else if (!value->is_null())
            {
                return value;
            }
        }

        const auto* parent_ref = dict->get_indirect_reference("Parent");
        if (parent_ref == nullptr)
            return nullptr;

        auto* parent = current->identity().owner().resolve_object(*parent_ref);
        if (parent == nullptr)
            return nullptr;

        current = parent;
    }
}

std::optional<geometry::rect> pages::media_box()
{
    const auto* value = inherited("MediaBox");
    if (value == nullptr)
        return std::nullopt;

    return page_attribute::read_box(obj().identity().owner(), *value);
}

std::optional<geometry::rect> pages::crop_box()
{
    const auto* value = inherited("CropBox");
    if (value == nullptr)
        return std::nullopt;

    return page_attribute::read_box(obj().identity().owner(), *value);
}

std::optional<int> pages::rotation()
{
    const auto* value = inherited("Rotate");
    if (value == nullptr)
        return std::nullopt;

    const auto* number = value->as_number();
    if (number == nullptr)
        throw logic_exception{"Page /Rotate is not a number"};

    const auto normalized = page_attribute::normalize_rotation(number->as_integer());
    if (normalized % 90 != 0)
        throw logic_exception{"Page /Rotate must be a multiple of 90 degrees"};

    return normalized;
}

std::optional<class page> pages::page(std::uint64_t index)
{
    if (index >= count())
        return std::nullopt;

    std::uint64_t remaining = index;

    std::unordered_set<indirect_reference> visited;
    const auto max_depth = obj().identity().owner().resource_limits().max_page_tree_depth;

    auto find = [&](this auto&& self, page_tree_node node,
                    std::size_t depth) -> std::optional<class page>
    {
        if (depth > max_depth)
            throw logic_exception{"Page tree depth exceeds the configured limit"};

        const auto node_ref = node.obj().identity().reference();
        if (!visited.insert(node_ref).second)
            throw logic_exception{"Cyclic page tree detected at object " +
                                  std::to_string(node_ref.object_number())};

        for (auto kid : node.children())
        {
            if (kid.is_leaf())
            {
                if (remaining == 0)
                    return kid.as_page();
                --remaining;
            }
            else
            {
                const auto sub = kid.subtree_count();
                if (remaining < sub)
                    return self(kid, depth + 1);
                remaining -= sub;
            }
        }
        return std::nullopt;
    };

    return find(page_tree_node{obj()}, 0);
}

std::optional<class page> pages::page(indirect_reference ref)
{
    auto* resolved = obj().identity().owner().resolve_object(ref);
    if (resolved == nullptr)
        return std::nullopt;

    auto* d = resolved->content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Page reference " + std::to_string(ref.object_number()) +
                              " is not a dictionary"};

    auto type = d->get_name("Type");
    if (type == nullptr || type->value != "Page")
        throw logic_exception{"Page reference " + std::to_string(ref.object_number()) +
                              " does not have /Type /Page"};

    return ripper::pdf::core::page{*resolved};
}

void pages::each(const std::function<void(class page&)>& callback)
{
    if (!callback)
        throw logic_exception{"Pages::each callback cannot be empty"};

    std::unordered_set<indirect_reference> visited;
    const auto max_depth = obj().identity().owner().resource_limits().max_page_tree_depth;

    auto walk = [&](this auto&& self, page_tree_node node, std::size_t depth) -> void
    {
        if (depth > max_depth)
            throw logic_exception{"Page tree depth exceeds the configured limit"};

        const auto node_ref = node.obj().identity().reference();
        if (!visited.insert(node_ref).second)
            throw logic_exception{"Cyclic page tree detected at object " +
                                  std::to_string(node_ref.object_number())};

        for (auto kid : node.children())
        {
            if (kid.is_leaf())
            {
                auto pg = *kid.as_page();
                callback(pg);
            }
            else
            {
                self(kid, depth + 1);
            }
        }
    };

    walk(page_tree_node{obj()}, 0);
}

class page pages::append_page(const geometry::rect& box)
{
    return insert_page(count(), box);
}

class page pages::append_page(page_size size, page_orientation orientation)
{
    return append_page(ripper::pdf::core::media_box(size, orientation));
}

class page pages::prepend_page(const geometry::rect& box)
{
    return insert_page(0, box);
}

class page pages::prepend_page(page_size size, page_orientation orientation)
{
    return prepend_page(ripper::pdf::core::media_box(size, orientation));
}

class page pages::insert_page(std::uint64_t index, const geometry::rect& box)
{
    rebind_to_active_revision();

    if (index > count())
        throw logic_exception{"Page index out of range"};

    auto& doc = obj().identity().owner();

    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Pages content is not a dictionary"};

    // Walk the children, skipping whole subtrees, until `remaining` reaches the
    // insertion point. `child_index` is the /Kids slot for this node; a subnode
    // that contains the insertion point is recursed into instead.
    std::uint64_t remaining = index;
    std::size_t child_index = 0;
    std::optional<pages> subtree;

    if (auto* kids = d->get_array("Kids"); kids != nullptr)
    {
        for (; child_index < kids->size(); ++child_index)
        {
            const auto* ref = (*kids)[child_index].as_indirect_reference();
            if (ref == nullptr)
                throw logic_exception{"Kid entry is not an indirect reference"};

            auto* resolved = doc.resolve_object(*ref);
            if (resolved == nullptr)
                throw logic_exception{"Failed to resolve kid reference"};

            page_tree_node kid{*resolved};

            if (kid.is_leaf())
            {
                if (remaining == 0)
                    break;

                --remaining;
                continue;
            }

            const auto subtree_size = kid.subtree_count();
            if (remaining == 0)
                break;

            if (remaining < subtree_size)
            {
                subtree = kid.as_pages();
                break;
            }

            remaining -= subtree_size;
        }
    }

    if (subtree)
        return subtree->insert_page(remaining, box);

    auto created = object_manager::create_page(doc, *this, box);
    insert_kid(*this, child_index, created.obj().identity().reference());
    bump_counts(page_tree_node{obj()}, 1);

    return created;
}

class page pages::insert_page(std::uint64_t index, page_size size, page_orientation orientation)
{
    return insert_page(index, ripper::pdf::core::media_box(size, orientation));
}

void pages::remove(std::uint64_t page_index)
{
    rebind_to_active_revision();

    auto target = page(page_index);
    if (!target)
        return;

    const auto& page_ref = target->obj().identity().reference();
    auto node = page_tree_node{target->obj()};

    // Remove from parent's /Kids and fix /Count on the parent and all ancestors.
    auto maybe_parent = node.parent();
    if (!maybe_parent)
        return;

    maybe_parent->remove_child(page_ref);
    bump_counts(*maybe_parent, -1);

    // Mark the xref entry as deleted so it is pruned on full rewrite and
    // threaded into the §7.5.4 free list for incremental saves.
    auto& xref = target->obj().identity().owner().cross_reference_table();
    xref.mark_deleted(page_ref);
}

void pages::prune(std::uint64_t page_index)
{
    throw logic_exception{"Not implemented: pages::prune"};
}
} // namespace ripper::pdf::core
