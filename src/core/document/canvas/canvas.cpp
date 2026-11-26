#include "ripper/pdf/core/document/canvas/canvas.hpp"

#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/content/content_stream_serializer.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] std::string_view rendering_intent_name(rendering_intent intent)
{
    switch (intent)
    {
        case rendering_intent::absolute_colorimetric:
            return "AbsoluteColorimetric";
        case rendering_intent::relative_colorimetric:
            return "RelativeColorimetric";
        case rendering_intent::saturation:
            return "Saturation";
        case rendering_intent::perceptual:
            return "Perceptual";
    }

    throw logic_exception{"Unknown rendering intent"};
}

[[nodiscard]] std::vector<object> components_as_objects(std::span<const double> components)
{
    std::vector<object> operands;
    operands.reserve(components.size());

    for (const double component : components)
        operands.emplace_back(component);

    return operands;
}
} // namespace

canvas::canvas(stream_object& stream) noexcept : stream_{stream} {}

canvas& canvas::op(std::string_view operator_name, std::vector<object> operands)
{
    return op(content_operation{std::string{operator_name}, std::move(operands)});
}

canvas& canvas::op(const content_operation& operation)
{
    auto bytes = content_stream_serializer::serialize(operation);

    stream_.write(bytes);

    return *this;
}

canvas& canvas::save()
{
    op(content_operator::q);

    ++state_depth_;
    return *this;
}

canvas& canvas::restore()
{
    if (state_depth_ == 0)
        throw logic_exception{"Unbalanced graphics state: Q without a matching q"};

    op(content_operator::Q);

    --state_depth_;
    return *this;
}

canvas& canvas::concat(const geometry::matrix& transformation)
{
    return op(content_operator::cm,
              {object{transformation.a}, object{transformation.b}, object{transformation.c},
               object{transformation.d}, object{transformation.e}, object{transformation.f}});
}

canvas& canvas::translate(double tx, double ty)
{
    return concat(geometry::matrix::translated(tx, ty));
}

canvas& canvas::scale(double sx, double sy)
{
    return concat(geometry::matrix::scaled(sx, sy));
}

canvas& canvas::rotate(double degrees)
{
    return concat(geometry::matrix::rotated(degrees));
}

canvas& canvas::skew(double x_degrees, double y_degrees)
{
    return concat(geometry::matrix::skewed(x_degrees, y_degrees));
}

canvas& canvas::set_line_width(double width)
{
    if (width < 0.0)
        throw logic_exception{"Line width must be non-negative"};

    return op(content_operator::w, {object{width}});
}

canvas& canvas::set_line_cap(line_cap cap)
{
    return op(content_operator::J, {object{static_cast<std::int64_t>(cap)}});
}

canvas& canvas::set_line_join(line_join join)
{
    return op(content_operator::j, {object{static_cast<std::int64_t>(join)}});
}

canvas& canvas::set_miter_limit(double limit)
{
    if (limit < 1.0)
        throw logic_exception{"Miter limit must be at least 1.0"};

    return op(content_operator::M, {object{limit}});
}

canvas& canvas::set_dash_pattern(std::span<const double> pattern, double phase)
{
    array_object::storage_type entries;
    entries.reserve(pattern.size());

    bool any_positive = false;
    for (const double value : pattern)
    {
        if (value < 0.0)
            throw logic_exception{"Dash pattern entries must be non-negative"};

        any_positive = any_positive || value > 0.0;
        entries.push_back(object{value});
    }

    if (!pattern.empty() && !any_positive)
        throw logic_exception{"Dash pattern must contain at least one non-zero entry"};

    return op(content_operator::d, {object{array_object{std::move(entries)}}, object{phase}});
}

canvas& canvas::set_dash_pattern(std::initializer_list<double> pattern, double phase)
{
    return set_dash_pattern(std::span<const double>{pattern.begin(), pattern.size()}, phase);
}

canvas& canvas::set_rendering_intent(rendering_intent intent)
{
    return op(content_operator::ri,
              {object{name_object{std::string{rendering_intent_name(intent)}}}});
}

canvas& canvas::set_flatness(double flatness)
{
    if (!std::isfinite(flatness) || flatness < 0.0 || flatness > 100.0)
        throw logic_exception{"Flatness tolerance must be finite and in the range [0, 100]"};

    return op(content_operator::i, {object{flatness}});
}

canvas& canvas::set_graphics_state(std::string_view name)
{
    return op(content_operator::gs, {object{name_object{std::string{name}}}});
}

canvas& canvas::set_fill_color(const color& value)
{
    const auto components = value.components();

    switch (value.get_model())
    {
        case color::model::gray:
            return op(content_operator::g, {object{components[0]}});
        case color::model::rgb:
            return op(content_operator::rg,
                      {object{components[0]}, object{components[1]}, object{components[2]}});
        case color::model::cmyk:
            return op(content_operator::k, {object{components[0]}, object{components[1]},
                                            object{components[2]}, object{components[3]}});
    }

    throw logic_exception{"Unknown colour model"};
}

canvas& canvas::set_stroke_color(const color& value)
{
    const auto components = value.components();

    switch (value.get_model())
    {
        case color::model::gray:
            return op(content_operator::G, {object{components[0]}});
        case color::model::rgb:
            return op(content_operator::RG,
                      {object{components[0]}, object{components[1]}, object{components[2]}});
        case color::model::cmyk:
            return op(content_operator::K, {object{components[0]}, object{components[1]},
                                            object{components[2]}, object{components[3]}});
    }

    throw logic_exception{"Unknown colour model"};
}

canvas& canvas::set_fill_color_space(std::string_view space)
{
    return op(content_operator::cs, {object{name_object{std::string{space}}}});
}

canvas& canvas::set_stroke_color_space(std::string_view space)
{
    return op(content_operator::CS, {object{name_object{std::string{space}}}});
}

canvas& canvas::set_fill_color(std::span<const double> components)
{
    return op(content_operator::sc, components_as_objects(components));
}

canvas& canvas::set_stroke_color(std::span<const double> components)
{
    return op(content_operator::SC, components_as_objects(components));
}

canvas& canvas::set_fill_color(std::span<const double> components, std::string_view pattern)
{
    auto operands = components_as_objects(components);
    operands.emplace_back(name_object{std::string{pattern}});

    return op(content_operator::scn, std::move(operands));
}

canvas& canvas::set_stroke_color(std::span<const double> components, std::string_view pattern)
{
    auto operands = components_as_objects(components);
    operands.emplace_back(name_object{std::string{pattern}});

    return op(content_operator::SCN, std::move(operands));
}

canvas& canvas::set_font(std::string_view font_name, double size)
{
    return op(content_operator::Tf, {object{name_object{std::string{font_name}}}, object{size}});
}

canvas& canvas::set_character_spacing(double spacing)
{
    return op(content_operator::Tc, {object{spacing}});
}

canvas& canvas::set_word_spacing(double spacing)
{
    return op(content_operator::Tw, {object{spacing}});
}

canvas& canvas::set_horizontal_scaling(double percent)
{
    return op(content_operator::Tz, {object{percent}});
}

canvas& canvas::set_leading(double leading)
{
    return op(content_operator::TL, {object{leading}});
}

canvas& canvas::set_text_rise(double rise)
{
    return op(content_operator::Ts, {object{rise}});
}

canvas& canvas::set_text_render_mode(text_render_mode mode)
{
    return op(content_operator::Tr, {object{static_cast<std::int64_t>(mode)}});
}

canvas& canvas::stroke_line(geometry::point from, geometry::point to)
{
    return path().move_to(from).line_to(to).stroke();
}

canvas& canvas::stroke_rect(const geometry::rect& rect)
{
    return path().add_rect(rect).stroke();
}

canvas& canvas::fill_rect(const geometry::rect& rect, fill_rule rule)
{
    return path().add_rect(rect).fill(rule);
}

canvas& canvas::draw_xobject(std::string_view name)
{
    return op(content_operator::Do, {object{name_object{std::string{name}}}});
}

canvas& canvas::paint_shading(std::string_view name)
{
    return op(content_operator::sh, {object{name_object{std::string{name}}}});
}

canvas& canvas::draw_image(const inline_image& image)
{
    auto bytes = content_stream_serializer::serialize(image);

    stream_.write(bytes);
    return *this;
}

canvas& canvas::mark_point(std::string_view tag)
{
    return op(content_operator::MP, {object{name_object{std::string{tag}}}});
}

canvas& canvas::mark_point(std::string_view tag, const dictionary_object& properties)
{
    return op(content_operator::DP, {object{name_object{std::string{tag}}}, object{properties}});
}

canvas& canvas::begin_marked_content(std::string_view tag)
{
    op(content_operator::BMC, {object{name_object{std::string{tag}}}});

    ++marked_content_depth_;
    return *this;
}

canvas& canvas::begin_marked_content(std::string_view tag, std::string_view properties_name)
{
    op(content_operator::BDC,
       {object{name_object{std::string{tag}}}, object{name_object{std::string{properties_name}}}});

    ++marked_content_depth_;
    return *this;
}

canvas& canvas::begin_marked_content(std::string_view tag, const dictionary_object& properties)
{
    op(content_operator::BDC, {object{name_object{std::string{tag}}}, object{properties}});

    ++marked_content_depth_;
    return *this;
}

canvas& canvas::end_marked_content()
{
    if (marked_content_depth_ == 0)
        throw logic_exception{"Unbalanced marked content: EMC without a matching BMC/BDC"};

    op(content_operator::EMC);

    --marked_content_depth_;
    return *this;
}

canvas& canvas::begin_compatibility()
{
    op(content_operator::BX);

    ++compatibility_depth_;
    return *this;
}

canvas& canvas::end_compatibility()
{
    if (compatibility_depth_ == 0)
        throw logic_exception{"Unbalanced compatibility section: EX without a matching BX"};

    op(content_operator::EX);

    --compatibility_depth_;
    return *this;
}

path canvas::path() noexcept
{
    // The member function hides the type name inside the class scope, so the
    // return object names the type through the enclosing namespace.
    return ripper::pdf::core::path{*this};
}

text canvas::text()
{
    if (in_text_)
        throw logic_exception{"A text object is already open: call text::end() first"};

    op(content_operator::BT);
    in_text_ = true;

    // The member function hides the type name inside the class scope, so the
    // return object names the type through the enclosing namespace.
    return ripper::pdf::core::text{*this};
}

stream_object& canvas::stream() noexcept
{
    return stream_;
}

std::size_t canvas::state_depth() const noexcept
{
    return state_depth_;
}

bool canvas::in_text() const noexcept
{
    return in_text_;
}

std::size_t canvas::marked_content_depth() const noexcept
{
    return marked_content_depth_;
}

std::size_t canvas::compatibility_depth() const noexcept
{
    return compatibility_depth_;
}
} // namespace ripper::pdf::core
