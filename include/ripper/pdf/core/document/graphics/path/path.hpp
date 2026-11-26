#pragma once

#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/graphics/style.hpp"

#include <initializer_list>
#include <span>

namespace ripper::pdf::core::graphics
{
class brush;

/// Fluent view over a path being built on a `brush` (§8.5.2).
///
/// Obtained from `brush::path()`. The view owns the state of the path it
/// builds: painting ends the path, after which the same view may start the
/// next one. Because that state lives here rather than in the brush, a fresh
/// view from `brush::path()` starts with no open path, and path operators
/// written through `brush::op()` are not visible to it.
///
/// Every construction call returns `*this` for chaining, while the terminal
/// painting calls return the owning brush so a chain can continue at brush
/// level:
///
/// ```cpp
/// painter.set_line_width(1.5)
///        .path()
///        .move_to({72.0, 720.0})
///        .line_to({300.0, 720.0})
///        .line_to({300.0, 600.0})
///        .close()
///        .fill_and_stroke()
///        .restore();
/// ```
///
/// The view enforces the path invariants before anything is written: segments
/// and painting need an open path, and painting terminates it. Misuse raises
/// `logic_exception`.
class path
{
public:
    path(const path&) = delete;
    path& operator=(const path&) = delete;
    path(path&&) = delete;
    path& operator=(path&&) = delete;
    ~path() = default;

    /// Append a subpath starting at `p` (`m`).
    path& move_to(geometry::point p);

    /// Append a straight segment from the current point to `p` (`l`).
    ///
    /// @throws logic_exception if no path is open.
    path& line_to(geometry::point p);

    /// Append a cubic Bézier curve from the current point with control points
    /// `c1` and `c2`, ending at `end` (`c`). Requires an open path.
    path& curve_to(geometry::point c1, geometry::point c2, geometry::point end);

    /// Append a cubic Bézier curve that reuses the current point as its first
    /// control point (`v`). Requires an open path.
    path& curve_to_replicated_start(geometry::point c2, geometry::point end);

    /// Append a cubic Bézier curve that reuses `end` as its second control
    /// point (`y`). Requires an open path.
    path& curve_to_replicated_end(geometry::point c1, geometry::point end);

    /// Close the current subpath (`h`).
    ///
    /// The current point becomes the start of the subpath, so further segments
    /// may be appended to it. @throws logic_exception if no path is open.
    path& close();

    /// Append a closed rectangular subpath (`re`).
    ///
    /// Rectangles are complete subpaths and may start a path on their own.
    path& add_rect(const geometry::rect& r);

    /// Append an open polyline through `points` (`m` then `l`).
    ///
    /// @throws logic_exception if fewer than two points are supplied.
    path& add_polyline(std::span<const geometry::point> points);

    /// Convenience overload for a brace-enclosed polyline.
    path& add_polyline(std::initializer_list<geometry::point> points);

    /// Append a closed polygon through `points` (`m`, `l`, then `h`).
    ///
    /// @throws logic_exception if fewer than three points are supplied.
    path& add_polygon(std::span<const geometry::point> points);

    /// Convenience overload for a brace-enclosed polygon.
    path& add_polygon(std::initializer_list<geometry::point> points);

    /// Append a circle as four cubic Bézier segments and close it.
    ///
    /// @throws logic_exception if `radius` is not positive.
    path& add_circle(geometry::point center, double radius);

    /// Append an ellipse as four cubic Bézier segments and close it.
    ///
    /// @throws logic_exception if either radius is not positive.
    path& add_ellipse(geometry::point center, double x_radius, double y_radius);

    /// Append a closed rounded rectangle.
    ///
    /// `radius` is clamped to at most half the shorter side; a radius of zero
    /// emits a plain rectangle.
    ///
    /// @throws logic_exception if the rectangle has a non-positive extent or
    ///         `radius` is negative.
    path& add_rounded_rect(const geometry::rect& r, double radius);

    /// Append an elliptical arc as cubic Bézier segments (`m` then `c`).
    ///
    /// The arc starts at `start_degrees` and sweeps by `sweep_degrees`; it is
    /// left open so the caller can `close()` it or continue the subpath. The
    /// sweep is split into segments of at most 90 degrees.
    ///
    /// @throws logic_exception if a radius is not positive.
    path& add_arc(geometry::point center, double x_radius, double y_radius, double start_degrees,
                  double sweep_degrees);

    /// Convenience overload for a circular arc.
    path& add_arc(geometry::point center, double radius, double start_degrees,
                  double sweep_degrees);

    /// Graphics-state parameters are legal at any point in a path, so the
    /// brush setters are forwarded here for chaining.
    path& set_line_width(double width);
    path& set_line_cap(line_cap cap);
    path& set_line_join(line_join join);
    path& set_miter_limit(double limit);
    path& set_dash_pattern(std::span<const double> pattern, double phase = 0.0);
    path& set_dash_pattern(std::initializer_list<double> pattern, double phase = 0.0);

    /// Paint the path with `style` and end it. Returns the owning brush.
    ///
    /// @throws logic_exception if no path is open.
    brush& paint(paint_style style);

    /// Stroke the path (`S`) and end it.
    brush& stroke();

    /// Fill the path (`f` or `f*`) and end it, implicitly closing subpaths.
    brush& fill(fill_rule rule = fill_rule::nonzero);

    /// Fill and stroke the path (`B` or `B*`) and end it.
    brush& fill_and_stroke(fill_rule rule = fill_rule::nonzero);

    /// Intersect the clip region with the path and end it (`W`/`W*` then `n`).
    brush& clip(fill_rule rule = fill_rule::nonzero);

    /// Discard the path without painting (`n`) and end it.
    brush& discard();

private:
    friend class brush;

    explicit path(brush& target) noexcept : brush_{&target} {}

    /// @throws logic_exception if this view has no open path.
    void ensure_open() const;

    brush* brush_;
    bool open_ = false;
};
} // namespace ripper::pdf::core::graphics
