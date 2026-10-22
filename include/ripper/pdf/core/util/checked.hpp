#pragma once

#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace ripper::pdf::core::checked
{
/// Return the greatest value of type `T`, or `fallback` if `T` is wider than `std::uint64_t`.
namespace detail
{
template <typename T> [[nodiscard]] constexpr std::uint64_t max_as_u64() noexcept
{
    if constexpr (sizeof(T) >= sizeof(std::uint64_t))
        return std::numeric_limits<std::uint64_t>::max();
    else
        return static_cast<std::uint64_t>(std::numeric_limits<T>::max());
}

template <typename T, typename U> [[nodiscard]] T to_unsigned_impl(U value, std::string_view what)
{
    static_assert(std::is_unsigned_v<U>, "to_unsigned_impl requires an unsigned input type");

    if constexpr (sizeof(U) > sizeof(std::uint64_t))
    {
        if (value > std::numeric_limits<std::uint64_t>::max())
            throw parse_exception{std::string{what} + ": value out of range"};
    }

    if (static_cast<std::uint64_t>(value) > max_as_u64<T>())
        throw parse_exception{std::string{what} + ": value out of range"};

    return static_cast<T>(value);
}
} // namespace detail

/// Add two `std::size_t` values, throwing `parse_exception` on overflow.
[[nodiscard]] inline std::size_t add(std::size_t a, std::size_t b, std::string_view what)
{
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw parse_exception{std::string{what} + ": size overflow"};

    return a + b;
}

/// Multiply two `std::size_t` values, throwing `parse_exception` on overflow.
[[nodiscard]] inline std::size_t multiply(std::size_t a, std::size_t b, std::string_view what)
{
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
        throw parse_exception{std::string{what} + ": size overflow"};

    return a * b;
}

/// Narrow an integral value to unsigned `T`, throwing `parse_exception` when it
/// is negative or out of range.
template <typename T, typename U>
    requires std::is_integral_v<U>
[[nodiscard]] inline T to_unsigned(U value, std::string_view what)
{
    static_assert(std::is_unsigned_v<T>, "to_unsigned requires an unsigned destination type");

    if constexpr (std::is_signed_v<U>)
    {
        if (value < 0)
            throw parse_exception{std::string{what} + ": value must not be negative"};

        return detail::to_unsigned_impl<T>(static_cast<std::make_unsigned_t<U>>(value), what);
    }
    else
    {
        return detail::to_unsigned_impl<T>(value, what);
    }
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
inline void require_range(std::size_t offset, std::size_t length, std::size_t size,
                          std::string_view what)
{
    if (offset > size || length > size - offset)
        throw parse_exception{std::string{what} + ": byte range out of bounds"};
}

} // namespace ripper::pdf::core::checked
