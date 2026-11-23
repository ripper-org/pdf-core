#include "ripper/pdf/core/document/canvas/canvas.hpp"

#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/document/object/number_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/content/content_stream_serializer.hpp"

#include <string>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] object real(double value)
{
    return object{number_object{value}};
}
} // namespace

canvas::canvas(stream_object& stream) noexcept : stream_{stream} {}

void canvas::op(std::string_view operator_name, std::vector<object> operands)
{
    op(content_operation{std::string{operator_name}, std::move(operands)});
}

void canvas::op(const content_operation& operation)
{
    auto bytes = content_stream_serializer::serialize(operation);

    stream_.write(bytes);
}

void canvas::save()
{
    op(content_operator::q);

    ++state_depth_;
}

void canvas::restore()
{
    if (state_depth_ == 0)
        throw logic_exception{"Unbalanced graphics state: Q without a matching q"};

    op(content_operator::Q);

    --state_depth_;
}

void canvas::concat(const geometry::matrix& transformation)
{
    op(content_operator::cm,
       {real(transformation.a), real(transformation.b), real(transformation.c),
        real(transformation.d), real(transformation.e), real(transformation.f)});
}

void canvas::translate(double tx, double ty)
{
    concat(geometry::matrix::translated(tx, ty));
}

void canvas::scale(double sx, double sy)
{
    concat(geometry::matrix::scaled(sx, sy));
}

void canvas::rotate(double degrees)
{
    concat(geometry::matrix::rotated(degrees));
}

void canvas::skew(double x_degrees, double y_degrees)
{
    concat(geometry::matrix::skewed(x_degrees, y_degrees));
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
