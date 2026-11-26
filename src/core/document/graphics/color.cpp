#include "ripper/pdf/core/document/graphics/color.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"

#include <cmath>
#include <string>
#include <string_view>

namespace ripper::pdf::core::graphics
{
namespace
{
void validate_component(double value, std::string_view name)
{
    if (!std::isfinite(value) || value < 0.0 || value > 1.0)
        throw logic_exception{std::string{"Color component "} + std::string{name} +
                              " must be finite and in the range [0, 1]"};
}
} // namespace

color color::gray(double level)
{
    validate_component(level, "gray");
    return color{model::gray, {level, 0.0, 0.0, 0.0}, 1};
}

color color::rgb(double red, double green, double blue)
{
    validate_component(red, "red");
    validate_component(green, "green");
    validate_component(blue, "blue");
    return color{model::rgb, {red, green, blue, 0.0}, 3};
}

color color::cmyk(double cyan, double magenta, double yellow, double black)
{
    validate_component(cyan, "cyan");
    validate_component(magenta, "magenta");
    validate_component(yellow, "yellow");
    validate_component(black, "black");
    return color{model::cmyk, {cyan, magenta, yellow, black}, 4};
}

color::color(model space, std::array<double, 4> components, std::size_t count) noexcept
    : model_{space}, components_{components}, component_count_{count}
{
}

color::model color::get_model() const noexcept
{
    return model_;
}

std::span<const double> color::components() const noexcept
{
    return {components_.data(), component_count_};
}
} // namespace ripper::pdf::core::graphics
