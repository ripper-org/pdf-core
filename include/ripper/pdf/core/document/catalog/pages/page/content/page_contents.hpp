#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"
#include "ripper/pdf/core/document/object/helpers/direct_object_view.hpp"

#include <cstddef>
#include <vector>

namespace ripper::pdf::core
{
class array_object;
class indirect_reference;
class object;
class page;

/// The /Contents entry of a page (ISO 32000-1 §7.7.3.3).
///
/// Facet of a `page`: a non-owning view over the page's /Contents *slot*. The
/// slot may hold null, a single indirect stream reference, or an array of
/// indirect stream references. All forms are exposed through the same API;
/// `is_array()` reports which form the document actually uses.
///
/// Mutators keep the entry in array form: appending, inserting, replacing or
/// removing never collapses it back to a single reference, and removing the
/// last stream leaves an empty array.
///
/// Because the view targets the slot rather than the current value, replacing
/// the value (for example promoting a reference to an array) does not
/// invalidate it.
///
/// Mutation happens in place on the page dictionary. The page must already be
/// bound to the active revision (`rebind_to_active_revision()`), otherwise the
/// change is not captured by an incremental save. This view cannot rebind
/// itself; obtain a fresh one from `page::content()` after rebinding.
///
/// The owning page's indirect object must outlive the view.
class page_contents : public direct_object_view
{
public:
    /// Returns a view over `owner`'s /Contents slot, materializing an empty array
    /// when the entry is absent.
    ///
    /// Does not allocate a content stream: an absent /Contents entry becomes an
    /// empty array, which the returned view reports as empty.
    ///
    /// The owner must already be bound to the active revision, otherwise the
    /// change is not captured by an incremental save.
    ///
    /// @return A non-owning view over the page's /Contents slot; the page must
    ///         outlive it.
    ///
    /// @throws logic_exception if the page object is not a dictionary.
    [[nodiscard]] static page_contents ensure(page& owner);

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
    /// The entry is stored as an array; a null or single-reference entry is
    /// promoted to an array.
    void append(content_stream stream);

    /// Inserts `stream` before position `index`.
    ///
    /// `index == size()` appends. The entry is always stored as an array.
    ///
    /// @throws std::out_of_range if `index > size()`.
    void insert(std::size_t index, content_stream stream);

    /// Replaces the stream reference at `index`.
    ///
    /// The entry is stored as an array.
    ///
    /// @throws std::out_of_range if `index >= size()`.
    void replace(std::size_t index, content_stream stream);

    /// Removes the stream reference at `index`.
    ///
    /// The entry stays an array; removing the last stream leaves an empty
    /// array.
    ///
    /// @throws std::out_of_range if `index >= size()`.
    void remove(std::size_t index);

    /// Removes every stream reference, leaving an empty array.
    void clear();

    /// Merges every stream in /Contents into one stream.
    ///
    /// This is the exceptional way to leave /Contents in its single-stream
    /// form. Streams are decoded in document order, concatenated with a single
    /// pre-sized buffer, and stored as a new unfiltered stream. /Contents is
    /// replaced by that stream's reference.
    ///
    /// An empty /Contents is a no-op. A single stream is collapsed to a
    /// reference without being re-encoded.
    ///
    /// @throws logic_exception if a reference is not a stream or decoding fails.
    void squash();

private:
    /// @param contents the page's /Contents slot.
    /// @param owner the page's indirect_object (must hold a dictionary).
    page_contents(object& contents, indirect_object& owner) noexcept;

    /// Returns /Contents as an array, promoting a single reference or a null
    /// value in place.
    [[nodiscard]] class array_object& ensure_array();

    /// Returns the reference stored at `index`, validating the entry shape.
    [[nodiscard]] const indirect_reference& reference_at(std::size_t index) const;
};
} // namespace ripper::pdf::core
