#include "ripper/pdf/core/document/canvas/path/path.hpp"

#include "ripper/pdf/core/document/canvas/canvas.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>

namespace ripper::pdf::core
{
namespace
{
/// Control-point distance for approximating a quarter circle with a cubic
/// Bézier curve.
constexpr double quarter_circle_kappa = 0.5522847498307936;

[[nodiscard]] geometry::point ellipse_point(geometry::point center, double x_radius,
                                            double y_radius, double radians) noexcept
{
    return {center.x + x_radius * std::cos(radians), center.y + y_radius * std::sin(radians)};
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
    canvas_->op(content_operator::m, {object{p.x}, object{p.y}});
    open_ = true;
    return *this;
}

path& path::line_to(geometry::point p)
{
    ensure_open();

    canvas_->op(content_operator::l, {object{p.x}, object{p.y}});
    return *this;
}

path& path::curve_to(geometry::point c1, geometry::point c2, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::c, {object{c1.x}, object{c1.y}, object{c2.x}, object{c2.y},
                                      object{end.x}, object{end.y}});
    return *this;
}

path& path::curve_to_replicated_start(geometry::point c2, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::v, {object{c2.x}, object{c2.y}, object{end.x}, object{end.y}});
    return *this;
}

path& path::curve_to_replicated_end(geometry::point c1, geometry::point end)
{
    ensure_open();

    canvas_->op(content_operator::y, {object{c1.x}, object{c1.y}, object{end.x}, object{end.y}});
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
    canvas_->op(content_operator::re,
                {object{r.x}, object{r.y}, object{r.width}, object{r.height}});
    open_ = true;
    return *this;
}

path& path::add_polyline(std::span<const geometry::point> points)
{
    if (points.size() < 2)
        throw logic_exception{"A polyline requires at least two points"};

    move_to(points.front());

    for (std::size_t i = 1; i < points.size(); ++i)
        line_to(points[i]);

    return *this;
}

path& path::add_polyline(std::initializer_list<geometry::point> points)
{
    return add_polyline(std::span<const geometry::point>{points.begin(), points.size()});
}

path& path::add_polygon(std::span<const geometry::point> points)
{
    if (points.size() < 3)
        throw logic_exception{"A polygon requires at least three points"};

    add_polyline(points);
    return close();
}

path& path::add_polygon(std::initializer_list<geometry::point> points)
{
    return add_polygon(std::span<const geometry::point>{points.begin(), points.size()});
}

path& path::add_circle(geometry::point center, double radius)
{
    return add_ellipse(center, radius, radius);
}

path& path::add_ellipse(geometry::point center, double x_radius, double y_radius)
{
    if (x_radius <= 0.0 || y_radius <= 0.0)
        throw logic_exception{"Ellipse radii must be positive"};

    const double x_control = quarter_circle_kappa * x_radius;
    const double y_control = quarter_circle_kappa * y_radius;

    move_to({center.x + x_radius, center.y});
    curve_to({center.x + x_radius, center.y + y_control},
             {center.x + x_control, center.y + y_radius}, {center.x, center.y + y_radius});
    curve_to({center.x - x_control, center.y + y_radius},
             {center.x - x_radius, center.y + y_control}, {center.x - x_radius, center.y});
    curve_to({center.x - x_radius, center.y - y_control},
             {center.x - x_control, center.y - y_radius}, {center.x, center.y - y_radius});
    curve_to({center.x + x_control, center.y - y_radius},
             {center.x + x_radius, center.y - y_control}, {center.x + x_radius, center.y});

    return close();
}

path& path::add_rounded_rect(const geometry::rect& r, double radius)
{
    if (r.width <= 0.0 || r.height <= 0.0)
        throw logic_exception{"Rounded rectangle requires positive width and height"};

    if (radius < 0.0)
        throw logic_exception{"Rounded rectangle radius must be non-negative"};

    if (radius == 0.0)
        return add_rect(r);

    radius = std::min(radius, std::min(r.width, r.height) / 2.0);

    const double control = quarter_circle_kappa * radius;
    const double right = r.x + r.width;
    const double top = r.y + r.height;

    move_to({r.x + radius, r.y});
    line_to({right - radius, r.y});
    curve_to({right - radius + control, r.y}, {right, r.y + radius - control},
             {right, r.y + radius});
    line_to({right, top - radius});
    curve_to({right, top - radius + control}, {right - radius + control, top},
             {right - radius, top});
    line_to({r.x + radius, top});
    curve_to({r.x + radius - control, top}, {r.x, top - radius + control}, {r.x, top - radius});
    line_to({r.x, r.y + radius});
    curve_to({r.x, r.y + radius - control}, {r.x + radius - control, r.y}, {r.x + radius, r.y});

    return close();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
path& path::add_arc(geometry::point center, double x_radius, double y_radius, double start_degrees,
                    double sweep_degrees)
{
    if (x_radius <= 0.0 || y_radius <= 0.0)
        throw logic_exception{"Arc radii must be positive"};

    if (!std::isfinite(start_degrees) || !std::isfinite(sweep_degrees) || sweep_degrees == 0.0)
        throw logic_exception{"Arc angles must be finite and the sweep must be non-zero"};

    const double start = start_degrees * std::numbers::pi / 180.0;
    const double sweep = sweep_degrees * std::numbers::pi / 180.0;

    const double quarter_turns = std::abs(sweep) / (std::numbers::pi / 2.0);
    const auto segments =
        std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(quarter_turns - 1e-9)));
    const double step = sweep / static_cast<double>(segments);

    for (std::size_t i = 0; i < segments; ++i)
    {
        const double from = start + step * static_cast<double>(i);
        const double to = from + step;
        const double control = 4.0 / 3.0 * std::tan(step / 4.0);

        const auto first = ellipse_point(center, x_radius, y_radius, from);
        const auto last = ellipse_point(center, x_radius, y_radius, to);

        const geometry::point first_control{
            first.x - control * x_radius * std::sin(from),
            first.y + control * y_radius * std::cos(from),
        };
        const geometry::point last_control{
            last.x + control * x_radius * std::sin(to),
            last.y - control * y_radius * std::cos(to),
        };

        if (i == 0)
            move_to(first);

        curve_to(first_control, last_control, last);
    }

    return *this;
}

path& path::add_arc(geometry::point center, double radius, double start_degrees,
                    double sweep_degrees)
{
    return add_arc(center, radius, radius, start_degrees, sweep_degrees);
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
