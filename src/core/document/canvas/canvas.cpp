#include "ripper/pdf/core/document/canvas/canvas.hpp"

#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/document/object/number_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/content/content_stream_serializer.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] object real(double value)
{
    // Normalize negative zero: `-0.0` is a valid real but reads as noise in
    // golden output (e.g. the `-0.0` produced by `matrix::rotated(0)`).
    return object{number_object{value == 0.0 ? 0.0 : value}};
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
              {real(transformation.a), real(transformation.b), real(transformation.c),
               real(transformation.d), real(transformation.e), real(transformation.f)});
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

    return op(content_operator::w, {real(width)});
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

    return op(content_operator::M, {real(limit)});
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
        entries.push_back(real(value));
    }

    if (!pattern.empty() && !any_positive)
        throw logic_exception{"Dash pattern must contain at least one non-zero entry"};

    return op(content_operator::d, {object{array_object{std::move(entries)}}, real(phase)});
}

canvas& canvas::set_dash_pattern(std::initializer_list<double> pattern, double phase)
{
    return set_dash_pattern(std::span<const double>{pattern.begin(), pattern.size()}, phase);
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

path canvas::path() noexcept
{
    // The member function hides the type name inside the class scope, so the
    // return object names the type through the enclosing namespace.
    return ripper::pdf::core::path{*this};
}

stream_object& canvas::stream() noexcept
{
    return stream_;
}

std::size_t canvas::state_depth() const noexcept
{
    return state_depth_;
}
} // namespace ripper::pdf::core
