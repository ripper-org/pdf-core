#pragma once

namespace ripper::pdf::core
{

/// Represents the PDF null object.
struct null_object
{
    /// All null objects compare equal.
    bool operator==(const null_object& other) const noexcept = default;
};

} // namespace ripper::pdf::core
