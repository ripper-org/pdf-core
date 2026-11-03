#pragma once

#include "ripper/pdf/core/util/byte_count.hpp"

#include <compare>
#include <cstddef>

namespace ripper::pdf::core
{
/// A validated byte position.
///
/// Offsets are distinct from counts at the type level: only offsets can be
/// advanced, and only the distance between two offsets produces a count. This
/// keeps expressions such as `offset + length` from silently mixing the two
/// concepts with unchecked integer arithmetic.
///
/// An offset is meaningful only relative to a particular buffer. The type
/// does not retain that association; use `checked_subspan()` to validate a
/// range against its buffer.
class byte_offset
{
public:
    /// The first byte of a buffer.
    [[nodiscard]] static constexpr byte_offset zero() noexcept
    {
        return byte_offset{0};
    }

    /// Construct an offset from an integral value.
    ///
    /// @throws parse_exception if the value is negative or does not fit in
    ///         `std::size_t`.
    template <checked_numeric::integer T>
    [[nodiscard]] static constexpr byte_offset from_integer(T value)
    {
        return byte_offset{checked_numeric::narrow<std::size_t>(value, "byte offset")};
    }

    /// The underlying position.
    [[nodiscard]] constexpr std::size_t value() const noexcept
    {
        return value_;
    }

    /// Advance this offset by `distance`, throwing on overflow.
    [[nodiscard]] constexpr byte_offset advanced_by(byte_count distance) const
    {
        return byte_offset{checked_numeric::add(value_, distance.value(), "byte offset")};
    }

    /// The number of bytes between this offset and `end`.
    ///
    /// @throws parse_exception if `end` precedes this offset.
    [[nodiscard]] constexpr byte_count distance_to(byte_offset end) const
    {
        return byte_count::from_integer(
            checked_numeric::subtract(end.value_, value_, "byte offset"));
    }

    friend constexpr auto operator<=>(const byte_offset&, const byte_offset&) = default;

private:
    explicit constexpr byte_offset(std::size_t value) noexcept : value_{value} {}

    std::size_t value_;
};
} // namespace ripper::pdf::core
