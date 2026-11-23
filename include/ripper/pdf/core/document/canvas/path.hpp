#pragma once

#include "ripper/pdf/core/document/canvas/style.hpp"
#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"

#include <initializer_list>
#include <span>

namespace ripper::pdf::core
{
class canvas;

/// Fluent view over a path being built on a `canvas` (§8.5.2).
///
/// Obtained from `canvas::path()`. The view owns the state of the path it
/// builds: painting ends the path, after which the same view may start the
/// next one. Because that state lives here rather than in the canvas, a fresh
/// view from `canvas::path()` starts with no open path, and path operators
/// written through `canvas::op()` are not visible to it.
///
/// Every construction call returns `*this` for chaining, while the terminal
/// painting calls return the owning canvas so a chain can continue at canvas
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

    /// Graphics-state parameters are legal at any point in a path, so the
    /// canvas setters are forwarded here for chaining.
    path& set_line_width(double width);
    path& set_line_cap(line_cap cap);
    path& set_line_join(line_join join);
    path& set_miter_limit(double limit);
    path& set_dash_pattern(std::span<const double> pattern, double phase = 0.0);
    path& set_dash_pattern(std::initializer_list<double> pattern, double phase = 0.0);

    /// Paint the path with `style` and end it. Returns the owning canvas.
    ///
    /// @throws logic_exception if no path is open.
    canvas& paint(paint_style style);

    /// Stroke the path (`S`) and end it.
    canvas& stroke();

    /// Fill the path (`f` or `f*`) and end it, implicitly closing subpaths.
    canvas& fill(fill_rule rule = fill_rule::nonzero);

    /// Fill and stroke the path (`B` or `B*`) and end it.
    canvas& fill_and_stroke(fill_rule rule = fill_rule::nonzero);

    /// Intersect the clip region with the path and end it (`W`/`W*` then `n`).
    canvas& clip(fill_rule rule = fill_rule::nonzero);

    /// Discard the path without painting (`n`) and end it.
    canvas& discard();

private:
    friend class canvas;

    explicit path(canvas& target) noexcept : canvas_{&target} {}

    /// @throws logic_exception if this view has no open path.
    void ensure_open() const;

    canvas* canvas_;
    bool open_ = false;
};
} // namespace ripper::pdf::core
