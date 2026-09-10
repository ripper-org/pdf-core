#pragma once

#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cmath>
#include <cstdint>
#include <variant>

namespace ripper::pdf::core
{

/// Represents a PDF numeric object: either an integer or a real.
///
/// The `std::variant` discriminator replaces the need for a separate kind
/// field: `std::holds_alternative<std::int64_t>` means integer,
/// `std::holds_alternative<double>` means real.
class number_object
{
public:
    /// Construct a default integer zero.
    number_object() noexcept : value_(std::int64_t{0}) {}

    /// Construct an integer.
    explicit number_object(std::int64_t v) noexcept : value_(v) {}

    /// Construct a real.
    /// @throws parse_exception if value is NaN or infinite.
    explicit number_object(double v) : value_(v)
    {
        if (!std::isfinite(v))
            throw parse_exception{"Non-finite double values (NaN/Inf) are not valid PDF reals"};
    }

    /// Returns `true` if this number holds an integer.
    [[nodiscard]] constexpr bool is_integer() const noexcept
    {
        return std::holds_alternative<std::int64_t>(value_);
    }

    /// Returns `true` if this number holds a real.
    [[nodiscard]] constexpr bool is_real() const noexcept
    {
        return std::holds_alternative<double>(value_);
    }

    /// Returns the value as `std::int64_t`, truncating if the value was
    /// originally a real.
    [[nodiscard]] constexpr std::int64_t as_integer() const noexcept
    {
        return is_integer() ? std::get<std::int64_t>(value_) : static_cast<std::int64_t>(as_real());
    }

    /// Returns the value as `double`, widening if the value was originally
    /// an integer.
    [[nodiscard]] constexpr double as_real() const noexcept
    {
        return is_real() ? std::get<double>(value_)
                         : static_cast<double>(std::get<std::int64_t>(value_));
    }

private:
    std::variant<std::int64_t, double> value_;
};

} // namespace ripper::pdf::core
