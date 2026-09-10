#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/page.hpp"
#include "ripper/pdf/core/document/catalog/pages/pages.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ripper::pdf::core
{
/// A navigable view over any node in the PDF page tree.
///
/// Wraps any `indirect_object` whose /Type is either /Pages (an intermediate node)
/// or /Page (a leaf), and exposes typed, null-safe navigation in both directions of
/// the tree together with narrowing casts to the concrete typed views.
///
/// ## Tree structure
///
/// A PDF page tree is a balanced tree where:
///   - Internal nodes have /Type /Pages and a /Kids array of child references.
///   - Leaf nodes have /Type /Page and carry the actual page content.
///   - Every non-root node has a /Parent reference pointing to its parent /Pages node.
///   - The root /Pages node is the one directly referenced by the document catalog.
///
/// ## Usage pattern
///
///   page_tree_node node{some_indirect_object};
///   if (node.is_leaf()) { auto pg = *node.as_page(); ... }
///   for (auto kid : node.children()) { ... }
///   if (auto par = node.parent()) { par->remove_child(node.obj().identity().reference()); }
class page_tree_node : public object_view
{
public:
    explicit page_tree_node(indirect_object& obj) noexcept;

    /// Returns true when this node is a /Type /Page leaf.
    [[nodiscard]] bool is_leaf() const;

    /// Returns true when this node is the document's root /Pages node,
    /// i.e. the entry directly referenced by the catalog /Pages key.
    [[nodiscard]] bool is_root() const;

    /// Returns the parent /Pages node wrapped as a `page_tree_node`, or
    /// `std::nullopt` if there is no /Parent entry or it cannot be resolved
    /// (which is the case for the root /Pages node).
    [[nodiscard]] std::optional<page_tree_node> parent();

    /// Returns the immediate children of this /Pages node in document order.
    ///
    /// Each element wraps a resolved /Page or /Pages kid.
    /// Returns an empty vector when called on a /Page leaf node.
    ///
    /// @throws `logic_exception` if any kid reference cannot be resolved.
    [[nodiscard]] std::vector<page_tree_node> children();

    /// Removes the child identified by `ref` from this node's /Kids array.
    ///
    /// Does NOT update /Count, the caller is responsible for decrementing
    /// /Count on this node and all ancestors after calling this method.
    ///
    /// @throws `logic_exception` if this node has no /Kids array.
    void remove_child(const indirect_reference& ref);

    /// Returns this node as a `pages` view, or `std::nullopt` if the node
    /// is not a /Type /Pages node.
    [[nodiscard]] std::optional<class pages> as_pages();

    /// Returns this node as a `page` view, or `std::nullopt` if the node
    /// is not a /Type /Page node.
    [[nodiscard]] std::optional<class page> as_page();

    /// Returns the total number of reachable /Page descendants.
    ///
    /// Reads /Count for /Pages nodes; returns 1 for /Page leaf nodes.
    ///
    /// @throws `logic_exception` if the /Count entry is missing on a /Pages node.
    [[nodiscard]] std::uint64_t subtree_count() const;
};
} // namespace ripper::pdf::core
