#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace ripper::pdf::core
{

/// Value-semantic owning indirection, modelled after C++26 `std::indirect`.
///
/// @TODO: Use std::indirect when available.
template <typename T> class indirect
{
public:
    /// Construct an empty instance.
    indirect() noexcept = default;

    /// Construct an instance owning `value`.
    ///
    /// @throws std::bad_alloc if allocating the pointee fails.
    explicit indirect(T value) : ptr_(std::make_unique<T>(std::move(value))) {}

    /// Deep-copy `other`. An empty instance stays empty.
    ///
    /// @throws std::bad_alloc if allocating the pointee fails.
    indirect(const indirect& other)
        : ptr_(other.ptr_ != nullptr ? std::make_unique<T>(*other.ptr_) : nullptr)
    {
    }

    /// Transfer ownership from `other`, leaving it empty.
    indirect(indirect&& other) noexcept = default;

    /// Deep-copy `other`'s pointee. Strongly exception safe: this instance keeps
    /// its previous value if the copy throws.
    indirect& operator=(const indirect& other)
    {
        if (this != &other)
            ptr_ = other.ptr_ != nullptr ? std::make_unique<T>(*other.ptr_) : nullptr;

        return *this;
    }

    /// Transfer ownership from `other`, leaving it empty.
    indirect& operator=(indirect&& other) noexcept = default;

    ~indirect() = default;

    /// Returns the pointee. Undefined if empty.
    [[nodiscard]] T& operator*() noexcept
    {
        return *ptr_;
    }

    /// Returns the pointee. Undefined if empty.
    [[nodiscard]] const T& operator*() const noexcept
    {
        return *ptr_;
    }

    /// Returns the pointee. Undefined if empty.
    [[nodiscard]] T* operator->() noexcept
    {
        return ptr_.get();
    }

    /// Returns the pointee. Undefined if empty.
    [[nodiscard]] const T* operator->() const noexcept
    {
        return ptr_.get();
    }

    /// Returns the owned pointer, or `nullptr` if empty.
    [[nodiscard]] T* get() noexcept
    {
        return ptr_.get();
    }

    /// Returns the owned pointer, or `nullptr` if empty.
    [[nodiscard]] const T* get() const noexcept
    {
        return ptr_.get();
    }

    /// Returns `false` if no pointee is owned (default-constructed or moved-from).
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    /// Destroy the pointee, leaving this instance empty.
    void reset() noexcept
    {
        ptr_.reset();
    }

    /// Exchange the owned pointees.
    void swap(indirect& other) noexcept
    {
        ptr_.swap(other.ptr_);
    }

private:
    std::unique_ptr<T> ptr_;
};

/// Value equality. Two empty instances compare equal; an empty instance never
/// equals a non-empty one. `noexcept` when `T`'s equality is `noexcept`.
template <typename T>
[[nodiscard]] bool operator==(const indirect<T>& lhs,
                              const indirect<T>& rhs) noexcept(noexcept(*lhs == *rhs))
{
    if (!lhs || !rhs)
        return !lhs && !rhs;

    return *lhs == *rhs;
}

template <typename T>
[[nodiscard]] bool operator==(const indirect<T>& value, std::nullptr_t) noexcept
{
    return !value;
}

template <typename T>
[[nodiscard]] bool operator==(std::nullptr_t, const indirect<T>& value) noexcept
{
    return !value;
}

} // namespace ripper::pdf::core
