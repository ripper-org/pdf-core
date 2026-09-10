#include "ripper/pdf/core/document/catalog/pages/pages.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/page_tree_node.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace ripper::pdf::core
{
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

std::optional<class page> pages::page(std::uint64_t index)
{
    if (index >= count())
        return std::nullopt;

    std::uint64_t remaining = index;

    std::unordered_set<indirect_reference> visited;

    std::function<std::optional<class page>(page_tree_node)> find;
    find = [&](page_tree_node node) -> std::optional<class page>
    {
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
                    return find(kid);
                remaining -= sub;
            }
        }
        return std::nullopt;
    };

    return find(page_tree_node{obj()});
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

    std::function<void(page_tree_node)> walk;
    walk = [&](page_tree_node node)
    {
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
                walk(kid);
            }
        }
    };

    walk(page_tree_node{obj()});
}

class page pages::add_page()
{
    rebind_to_active_revision();

    auto& doc = obj().identity().owner();
    auto& xref = doc.cross_reference_table();

    const auto page_ref = xref.reserve();

    // This should in fact be a parameter
    array_object mediabox;
    mediabox.push_back(object{std::int64_t{0}});
    mediabox.push_back(object{std::int64_t{0}});
    mediabox.push_back(object{std::int64_t{612}});
    mediabox.push_back(object{std::int64_t{792}});

    ripper::pdf::core::dictionary_object page_dict;
    page_dict.set("Type", object{name_object{"Page"}});
    page_dict.set("Parent", object{obj().identity().reference()});
    page_dict.set("MediaBox", object{std::move(mediabox)});

    auto page_obj = std::make_unique<indirect_object>(object_identity{&doc, page_ref},
                                                      object{std::move(page_dict)});

    auto* raw_page = xref.commit(page_ref, std::move(page_obj));
    if (raw_page == nullptr)
        throw logic_exception{"Failed to commit page to cross-reference table"};

    // Update /Kids and /Count on this pages node
    auto* d = obj().content().as_dictionary();
    if (d == nullptr)
        throw logic_exception{"Pages content is not a dictionary"};

    if (d->get_array("Kids") == nullptr)
        d->set("Kids", object{array_object{}});

    auto* kids = d->get_array("Kids");
    kids->push_back(object{page_ref});

    std::uint64_t count = 0;

    const auto* count_ptr = d->get_number("Count");
    if (count_ptr != nullptr)
        count = static_cast<std::uint64_t>(count_ptr->as_integer());

    d->set("Count", object{static_cast<std::int64_t>(count + 1)});

    auto node = page_tree_node{obj()};
    std::unordered_set<indirect_reference> visited;
    while (true)
    {
        const auto node_ref = node.obj().identity().reference();
        if (!visited.insert(node_ref).second)
            throw logic_exception{"Cyclic /Parent chain detected while climbing the page tree"};

        node.rebind_to_active_revision();

        auto up = node.parent();
        if (!up)
            break;

        up->rebind_to_active_revision();

        auto* pd = up->obj().content().as_dictionary();
        if (pd == nullptr)
            break;

        auto* pc = pd->get_number("Count");
        if (pc == nullptr)
            break;

        pd->set("Count", object{pc->as_integer() + 1});

        if (up->is_root())
            break;

        node = *up;
    }

    return ripper::pdf::core::page{*raw_page};
}

void pages::delete_page(std::uint64_t page_index)
{
    rebind_to_active_revision();

    auto pg = this->page(page_index);
    if (!pg)
        return;

    const auto& page_ref = pg->obj().identity().reference();
    auto node = page_tree_node{pg->obj()};

    // Remove from parent's /Kids
    auto maybe_parent = node.parent();
    if (!maybe_parent)
        return;

    maybe_parent->remove_child(page_ref);

    // Decrement /Count on the immediate parent and every ancestor up to the root
    auto ancestor = *maybe_parent;
    std::unordered_set<indirect_reference> visited;
    while (true)
    {
        const auto ancestor_ref = ancestor.obj().identity().reference();
        if (!visited.insert(ancestor_ref).second)
            throw logic_exception{"Cyclic /Parent chain detected while climbing the page tree"};

        ancestor.rebind_to_active_revision();

        auto* d = ancestor.obj().content().as_dictionary();
        if (d == nullptr)
            break;

        auto* count_ptr = d->get_number("Count");
        if (count_ptr == nullptr || count_ptr->as_integer() <= 0)
            break;

        d->set("Count", object{count_ptr->as_integer() - 1});

        if (ancestor.is_root())
            break;

        auto up = ancestor.parent();
        if (!up)
            break;

        ancestor = *up;
    }

    // Mark the xref entry as deleted so it is pruned on full rewrite and
    // threaded into the §7.5.4 free list for incremental saves.
    auto& xref = pg->obj().identity().owner().cross_reference_table();
    xref.mark_deleted(page_ref);
}

void pages::prune_page(std::uint64_t page_index)
{
    throw logic_exception{"Not implemented: pages::prune_page"};
}
} // namespace ripper::pdf::core
