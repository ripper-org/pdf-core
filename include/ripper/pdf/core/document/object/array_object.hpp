#pragma once

#include <cstddef>
#include <vector>

namespace ripper::pdf::core
{

class object;

/// Represents a PDF array object: an ordered sequence of PDF objects.
///
/// Stored by value inside `object`'s variant.  Uses `std::vector<object>`
/// which tolerates incomplete `object` on common standard library
/// implementations, avoiding the mutual-recursion issue that forces
/// `dictionary_object` and `stream_object` to use `unique_ptr`.
class array_object
{
public:
    /// Type alias for the underlying storage.
    using storage_type = std::vector<object>;

    /// Iterator types for range-based usage.
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    /// Construct an empty array.
    array_object() noexcept = default;

    /// Construct an array from an existing vector of objects.
    explicit array_object(storage_type items) noexcept;

    array_object(const array_object& other);
    array_object(array_object&&) noexcept = default;
    array_object& operator=(const array_object& other);
    array_object& operator=(array_object&&) noexcept = default;
    ~array_object();

    /// Append an object to the end of the array.
    void push_back(object value);

    /// Access the object at `index`.  No bounds checking.
    [[nodiscard]] const object& operator[](std::size_t index) const;
    [[nodiscard]] object& operator[](std::size_t index);

    /// Access the object at `index` with bounds checking.  Throws
    /// `std::out_of_range` if out of bounds.
    [[nodiscard]] const object& at(std::size_t index) const;
    [[nodiscard]] object& at(std::size_t index);

    /// Returns the number of objects in the array.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns `true` if the array has no elements.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns the raw underlying vector for full traversal or serialization.
    [[nodiscard]] const storage_type& items() const noexcept;

    /// Returns a mutable reference to the raw underlying vector.
    [[nodiscard]] storage_type& items() noexcept;

    /// Returns an iterator to the beginning.
    [[nodiscard]] iterator begin() noexcept;

    /// Returns a const iterator to the beginning.
    [[nodiscard]] const_iterator begin() const noexcept;

    /// Returns an iterator to the end.
    [[nodiscard]] iterator end() noexcept;

    /// Returns a const iterator to the end.
    [[nodiscard]] const_iterator end() const noexcept;

    /// Erase the element at `it`.  Returns the iterator following the removed element.
    iterator erase(const_iterator it);

private:
    storage_type items_;
};

} // namespace ripper::pdf::core
