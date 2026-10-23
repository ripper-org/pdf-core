#pragma once

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/checked_numeric.hpp"

#include <compare>
#include <cstdint>

namespace ripper::pdf::core
{
/// The width, in bytes, of one field in an xref stream entry.
///
/// PDF 32000-1 §7.5.8 defines each `/W` element as an integer from 0 through
/// 8, where 0 means the field contributes no bytes. Wrapping the width in a
/// validated type keeps arbitrary integers away from the field reader and the
/// entry-size calculation.
class xref_field_width
{
public:
    /// The maximum width permitted by the specification.
    static constexpr std::uint8_t max_width = 8;

    /// Construct a field width from an integral value.
    ///
    /// @throws parse_exception if the value is negative, does not fit in
    ///         `std::uint8_t`, or is greater than `max_width`.
    template <checked_numeric::integer T>
    [[nodiscard]] static constexpr xref_field_width from_integer(T value)
    {
        const auto width = checked_numeric::narrow<std::uint8_t>(value, "xref field width");

        if (width > max_width)
            throw parse_exception{"xref field width must be between 0 and 8"};

        return xref_field_width{width};
    }

    /// The width in bytes.
    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return value_;
    }

    friend constexpr auto operator<=>(const xref_field_width&, const xref_field_width&) = default;

private:
    explicit constexpr xref_field_width(std::uint8_t value) noexcept : value_{value} {}

    std::uint8_t value_;
};
} // namespace ripper::pdf::core
