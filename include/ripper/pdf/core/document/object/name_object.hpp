#pragma once

#include <string>
#include <string_view>

namespace ripper::pdf::core
{

/// Represents a PDF name object (e.g. `/Type`, `/Pages`).
///
/// Distinct from `string_object` to preserve the semantic difference between
/// PDF name objects and PDF string objects at the type level.
///
/// Keys are stored without the leading `/` (e.g. `"Type"`, not `"/Type"`).
struct name_object
{
    std::string value;

    /// Equality comparison. Two names are equal if their key text matches.
    ///
    /// Compared as `string_view` so the comparison is genuinely `noexcept`,
    /// which keeps `object::operator==`'s `noexcept` honest.
    bool operator==(const name_object& other) const noexcept
    {
        return std::string_view{value} == std::string_view{other.value};
    }
};

} // namespace ripper::pdf::core
