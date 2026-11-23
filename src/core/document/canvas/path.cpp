#include "ripper/pdf/core/document/canvas/path.hpp"

#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/document/object/number_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <string_view>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] object real(double value)
{
    // Match the canvas: avoid emitting negative zero for path coordinates.
    return object{number_object{value == 0.0 ? 0.0 : value}};
}

[[nodiscard]] std::string_view painting_operator(paint_style style)
{
    switch (style)
    {
        case paint_style::stroke:
            return content_operator::S;
        case paint_style::close_stroke:
            return content_operator::s;
        case paint_style::fill:
            return content_operator::f;
        case paint_style::fill_even_odd:
            return content_operator::f_star;
        case paint_style::fill_stroke:
            return content_operator::B;
        case paint_style::fill_stroke_even_odd:
            return content_operator::B_star;
        case paint_style::close_fill_stroke:
            return content_operator::b;
        case paint_style::close_fill_stroke_even_odd:
            return content_operator::b_star;
    }

    throw logic_exception{"Unknown path paint style"};
}
} // namespace

path& path::move_to(geometry::point p)
{
    canvas_->op(content_operator::m, {real(p.x), real(p.y)});
    open_ = true;
    return *this;
}

path& path::line_to(geometry::point p)
{
    ensure_open();

    canvas_->op(content_operator::l, {real(p.x), real(p.y)});
    return *this;
}

path& path::curve_to(geometry::point c1, geometry::point c2, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::c,
                {real(c1.x), real(c1.y), real(c2.x), real(c2.y), real(end.x), real(end.y)});
    return *this;
}

path& path::curve_to_replicated_start(geometry::point c2, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::v, {real(c2.x), real(c2.y), real(end.x), real(end.y)});
    return *this;
}

path& path::curve_to_replicated_end(geometry::point c1, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::y, {real(c1.x), real(c1.y), real(end.x), real(end.y)});
    return *this;
}

path& path::close()
{
    ensure_open();

    canvas_->op(content_operator::h);
    return *this;
}

path& path::add_rect(const geometry::rect& r)
{
    canvas_->op(content_operator::re, {real(r.x), real(r.y), real(r.width), real(r.height)});
    open_ = true;
    return *this;
}

path& path::set_line_width(double width)
{
    canvas_->set_line_width(width);
    return *this;
}

path& path::set_line_cap(line_cap cap)
{
    canvas_->set_line_cap(cap);
    return *this;
}

path& path::set_line_join(line_join join)
{
    canvas_->set_line_join(join);
    return *this;
}

path& path::set_miter_limit(double limit)
{
    canvas_->set_miter_limit(limit);
    return *this;
}

path& path::set_dash_pattern(std::span<const double> pattern, double phase)
{
    canvas_->set_dash_pattern(pattern, phase);
    return *this;
}

path& path::set_dash_pattern(std::initializer_list<double> pattern, double phase)
{
    canvas_->set_dash_pattern(pattern, phase);
    return *this;
}

canvas& path::paint(paint_style style)
{
    ensure_open();

    auto& painted = canvas_->op(painting_operator(style));
    open_ = false;

    return painted;
}

canvas& path::stroke()
{
    return paint(paint_style::stroke);
}

canvas& path::fill(fill_rule rule)
{
    return paint(rule == fill_rule::nonzero ? paint_style::fill : paint_style::fill_even_odd);
}

canvas& path::fill_and_stroke(fill_rule rule)
{
    return paint(rule == fill_rule::nonzero ? paint_style::fill_stroke
                                            : paint_style::fill_stroke_even_odd);
}

canvas& path::clip(fill_rule rule)
{
    ensure_open();

    canvas_->op(rule == fill_rule::nonzero ? content_operator::W : content_operator::W_star);

    auto& clipped = canvas_->op(content_operator::n);
    open_ = false;

    return clipped;
}

canvas& path::discard()
{
    ensure_open();

    auto& discarded = canvas_->op(content_operator::n);
    open_ = false;

    return discarded;
}

void path::ensure_open() const
{
    if (!open_)
        throw logic_exception{
            "Path operation requires an open path: call move_to() or add_rect() first"};
}
} // namespace ripper::pdf::core
