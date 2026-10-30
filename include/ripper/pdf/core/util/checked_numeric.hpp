#pragma once

#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ripper::pdf::core::checked_numeric
{
/// Integral types accepted by the checked conversion and arithmetic helpers.
///
/// `bool` and the character types that `std::in_range` does not support
/// (`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) are excluded.
/// `signed char` and `unsigned char` remain valid integer types.
template <typename T>
concept integer = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
                  !std::same_as<T, wchar_t> && !std::same_as<T, char8_t> &&
                  !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

template <typename T>
concept unsigned_integer = integer<T> && std::unsigned_integral<T>;

/// Convert between integral types, throwing `parse_exception` when `value`
/// cannot be represented by `To`.
///
/// This is the single place where checked narrowing lives. Factories such as
/// `byte_count::from_integer()` delegate here so the supported range rules are
/// identical everywhere.
template <integer To, integer From>
[[nodiscard]] constexpr To narrow(From value, std::string_view what = "integer")
{
    if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>)
    {
        if (value < 0)
            throw parse_exception{std::string{what} + ": value must not be negative"};
    }

    if (!std::in_range<To>(value))
        throw parse_exception{std::string{what} + ": value out of range"};

    return static_cast<To>(value);
}

/// Add two `std::size_t` quantities, throwing `parse_exception` on overflow.
[[nodiscard]] constexpr std::size_t add(std::size_t a, std::size_t b, std::string_view what)
{
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw parse_exception{std::string{what} + ": size overflow"};

    return a + b;
}

/// Subtract `b` from `a`, throwing `parse_exception` when the result is negative.
[[nodiscard]] constexpr std::size_t subtract(std::size_t a, std::size_t b, std::string_view what)
{
    if (b > a)
        throw parse_exception{std::string{what} + ": size underflow"};

    return a - b;
}

/// Multiply two `std::size_t` quantities, throwing `parse_exception` on overflow.
[[nodiscard]] constexpr std::size_t multiply(std::size_t a, std::size_t b, std::string_view what)
{
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
        throw parse_exception{std::string{what} + ": size overflow"};

    return a * b;
}

/// Convert a real to `std::int64_t`, truncating toward zero.
///
/// Throws `parse_exception` for non-finite values and values outside the
/// representable `std::int64_t` range. The upper bound is exclusive because
/// `INT64_MAX` is not representable as a `double` (it rounds to 2^63).
[[nodiscard]] inline std::int64_t real_to_int64(double value, std::string_view what)
{
    // 2^63. The upper bound is exclusive: INT64_MAX rounds up to this value
    // when converted to double.
    constexpr double int64_limit = 9223372036854775808.0;

    if (!std::isfinite(value) || value < -int64_limit || value >= int64_limit)
        throw parse_exception{std::string{what} +
                              ": real value cannot be represented as an integer"};

    return static_cast<std::int64_t>(value);
}

/// Validate that `[offset, offset + length)` is contained in a buffer of `size` bytes.
constexpr void require_range(std::size_t offset, std::size_t length, std::size_t size,
                             std::string_view what)
{
    if (offset > size || length > size - offset)
        throw parse_exception{std::string{what} + " is out of bounds"};
}

} // namespace ripper::pdf::core::checked_numeric
