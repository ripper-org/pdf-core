#pragma once

#include "ripper/pdf/core/util/checked_numeric.hpp"

#include <compare>
#include <cstddef>

namespace ripper::pdf::core
{
/// A validated quantity of bytes.
///
/// The representation is private and the only way to obtain one from an
/// integer is `from_integer()`, which validates the value. Arithmetic returns
/// another `byte_count`, so overflow protection cannot silently be dropped by
/// falling back to a plain integer.
///
/// The type is a regular value: copyable, comparable, and `constexpr` for
/// quantities known at compile time.
class byte_count
{
public:
    /// A zero byte count.
    [[nodiscard]] static constexpr byte_count zero() noexcept
    {
        return byte_count{0};
    }

    /// Construct a byte count from an integral value.
    ///
    /// @throws parse_exception if the value is negative or does not fit in
    ///         `std::size_t`.
    template <checked_numeric::integer T>
    [[nodiscard]] static constexpr byte_count from_integer(T value)
    {
        return byte_count{checked_numeric::narrow<std::size_t>(value, "byte count")};
    }

    /// The underlying number of bytes.
    [[nodiscard]] constexpr std::size_t value() const noexcept
    {
        return value_;
    }

    /// Add another count, throwing on overflow.
    [[nodiscard]] constexpr byte_count plus(byte_count other) const
    {
        return byte_count{checked_numeric::add(value_, other.value_, "byte count")};
    }

    /// Subtract another count, throwing if the result would be negative.
    [[nodiscard]] constexpr byte_count minus(byte_count other) const
    {
        return byte_count{checked_numeric::subtract(value_, other.value_, "byte count")};
    }

    /// Multiply by a non-negative factor, throwing on overflow or a negative
    /// factor.
    template <checked_numeric::integer T>
    [[nodiscard]] constexpr byte_count multiplied_by(T factor) const
    {
        return byte_count{checked_numeric::multiply(
            value_, checked_numeric::narrow<std::size_t>(factor, "byte count factor"),
            "byte count")};
    }

    friend constexpr auto operator<=>(const byte_count&, const byte_count&) = default;

private:
    explicit constexpr byte_count(std::size_t value) noexcept : value_{value} {}

    std::size_t value_;
};
} // namespace ripper::pdf::core
