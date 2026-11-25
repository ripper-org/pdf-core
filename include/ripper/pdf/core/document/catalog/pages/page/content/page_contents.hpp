#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"
#include "ripper/pdf/core/document/object/helpers/object_view.hpp"

#include <cstddef>
#include <vector>

namespace ripper::pdf::core
{
class array_object;
class dictionary_object;
class indirect_reference;
class object;

/// The /Contents entry of a page (ISO 32000-1 §7.7.3.3).
///
/// Facet of a `page`: a non-owning view over the page's indirect_object that
/// exposes its /Contents entry as a single-or-multiple stream container, with
/// proxies for the common structural edits (append, insert, replace, remove).
///
/// /Contents is either absent/null, a single indirect stream reference, or an
/// array of indirect stream references. All forms are exposed through the same
/// API; `is_array()` reports which form the document actually uses and the
/// mutators promote a single reference to an array when a second stream is added.
///
/// Mutation happens in place on the page dictionary. The page must already be
/// bound to the active revision (`rebind_to_active_revision()`), otherwise the
/// change is not captured by an incremental save.
class page_contents : public object_view
{
public:
    /// @param page_obj the page's indirect_object (must hold a dictionary).
    explicit page_contents(indirect_object& page_obj) noexcept;

    /// Returns `true` if /Contents is absent, null or an empty array.
    [[nodiscard]] bool empty() const;

    /// Returns `true` if /Contents is stored as an array, even a one-element one.
    ///
    /// For simplicity's sake, the library considers an array with a single
    /// element the same as a single stream.
    [[nodiscard]] bool is_array() const;

    /// Returns the number of streams referenced by /Contents.
    ///
    /// A single reference counts as one, an absent or null entry as zero.
    ///
    /// @throws logic_exception if /Contents is neither a reference nor an array.
    [[nodiscard]] std::size_t size() const;

    /// Resolves the stream at `index`, in document order.
    ///
    /// @throws std::out_of_range if `index >= size()`.
    /// @throws logic_exception if /Contents is malformed or the entry does not
    ///         reference a stream.
    [[nodiscard]] content_stream at(std::size_t index) const;

    /// Resolves all referenced streams, in document order.
    ///
    /// Returns an empty vector when /Contents is absent or null.
    ///
    /// @throws logic_exception if /Contents is malformed or an entry does not
    ///         reference a stream.
    [[nodiscard]] std::vector<content_stream> streams() const;

    /// Appends `stream` to the end of /Contents.
    ///
    /// An absent or null entry becomes a single reference; a single reference
    /// is promoted to an array.
    void append(content_stream stream);

    /// Inserts `stream` before position `index`.
    ///
    /// `index == size()` appends. Promotes the entry to an array when needed.
    ///
    /// @throws std::out_of_range if `index > size()`.
    void insert(std::size_t index, content_stream stream);

    /// Replaces the stream reference at `index`.
    ///
    /// @throws std::out_of_range if `index >= size()`.
    void replace(std::size_t index, content_stream stream);

    /// Removes the stream reference at `index`.
    ///
    /// Collapses the entry back to a single reference when one stream remains,
    /// and to null when none remain.
    ///
    /// @throws std::out_of_range if `index >= size()`.
    void remove(std::size_t index);

    /// Removes every stream reference, leaving /Contents null.
    void clear();

private:
    /// Returns the /Contents entry, or `nullptr` if absent or the page object
    /// is not a dictionary.
    [[nodiscard]] class object* contents();
    [[nodiscard]] const class object* contents() const;

    /// Returns the page dictionary.
    ///
    /// @throws logic_exception if the page object is not a dictionary.
    [[nodiscard]] class dictionary_object& dictionary();

    /// Returns /Contents as an array, promoting a single reference or a null
    /// value in place and creating the entry when absent.
    [[nodiscard]] class array_object& ensure_array();

    /// Returns the reference stored at `index`, validating the entry shape.
    [[nodiscard]] const indirect_reference& reference_at(std::size_t index) const;
};
} // namespace ripper::pdf::core
