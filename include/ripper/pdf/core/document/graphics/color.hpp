#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace ripper::pdf::core
{
/// A device color value for the stroking or nonstroking color operators
/// (ISO 32000-1 §8.6.4).
///
/// The model determines which operator the canvas emits: `g`, `rg`, or `k`
/// for nonstroking and `G`, `RG`, or `K` for stroking. Components are validated
/// against the spec range `[0, 1]` when the value is created.
///
/// ```cpp
/// painter.set_fill_color(color::rgb(1.0, 0.0, 0.0));
/// painter.set_stroke_color(color::gray(0.25));
/// ```
class color
{
public:
    /// Device color model.
    enum class model
    {
        gray,
        rgb,
        cmyk
    };

    /// Create a DeviceGray color.
    ///
    /// @throws logic_exception if `level` is outside `[0, 1]` or not finite.
    [[nodiscard]] static color gray(double level);

    /// Create a DeviceRGB color.
    ///
    /// @throws logic_exception if any component is outside `[0, 1]` or not finite.
    [[nodiscard]] static color rgb(double red, double green, double blue);

    /// Create a DeviceCMYK color.
    ///
    /// @throws logic_exception if any component is outside `[0, 1]` or not finite.
    [[nodiscard]] static color cmyk(double cyan, double magenta, double yellow, double black);

    /// Returns the color model of this value.
    [[nodiscard]] model get_model() const noexcept;

    /// Returns the components for this model (`1`, `3`, or `4` entries).
    [[nodiscard]] std::span<const double> components() const noexcept;

private:
    color(model space, std::array<double, 4> components, std::size_t count) noexcept;

    model model_;
    std::array<double, 4> components_{};
    std::size_t component_count_ = 0;
};
} // namespace ripper::pdf::core
