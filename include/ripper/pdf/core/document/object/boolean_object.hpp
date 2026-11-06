#pragma once

namespace ripper::pdf::core
{

/// Represents a PDF boolean object (`true` or `false`).
///
/// A thin wrapper around `bool` that gives booleans class identity as a
/// PDF object type.  The implicit `operator bool()` allows transparent
/// use in conditionals and comparisons.
struct boolean_object
{
    /// The underlying boolean value.
    bool value;

    /// Construct a `false` boolean object.
    boolean_object() noexcept : value(false) {}

    /// Construct from a plain `bool`.
    explicit boolean_object(bool v) noexcept : value(v) {}

    /// Implicit conversion to `bool` for ergonomic use in conditionals.
    operator bool() const noexcept
    {
        return value;
    }

    /// Equality comparison. Two booleans are equal if their values match.
    bool operator==(const boolean_object& other) const noexcept = default;
};

} // namespace ripper::pdf::core
