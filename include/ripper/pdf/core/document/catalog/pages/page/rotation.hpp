#pragma once

#include <cstdint>

namespace ripper::pdf::core
{
class object;

/// A validated page /Rotate value.
///
/// Holds one of 0, 90, 180 or 270 degrees clockwise, so an instance always
/// represents a legal page rotation (ISO 32000-1 §7.7.3.3).
class page_rotation
{
public:
    /// Normalizes `degrees` into [0, 360) and requires a multiple of 90.
    ///
    /// @throws logic_exception if `degrees` is not a multiple of 90.
    explicit page_rotation(std::int64_t degrees);

    /// Reads /Rotate from an already-resolved object.
    ///
    /// @throws logic_exception if `value` is not a number or is not a multiple
    ///         of 90.
    [[nodiscard]] static page_rotation from(const object& value);

    /// Returns the rotation in degrees: 0, 90, 180 or 270.
    [[nodiscard]] int degrees() const noexcept;

    /// Returns true for 90 or 270 degrees, where the page is turned by an odd
    /// quarter turn.
    [[nodiscard]] bool is_quarter_turn() const noexcept;

    bool operator==(const page_rotation& other) const noexcept = default;

private:
    int degrees_;
};
} // namespace ripper::pdf::core
