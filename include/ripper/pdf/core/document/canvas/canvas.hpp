#pragma once

#include "ripper/pdf/core/document/canvas/path.hpp"
#include "ripper/pdf/core/document/canvas/style.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/content_item.hpp"
#include "ripper/pdf/core/document/geometry/matrix.hpp"
#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{
class stream_object;

/// Where a new content stream is placed in the page's /Contents entry.
enum class canvas_mode
{
    append,
    prepend,
    replace
};

/// Writer for one page content stream (ISO 32000-1 §8.2).
///
/// Thin, non-owning view over a `stream_object`: every call serializes a single
/// complete operator and appends it to the stream, keeping `/Length` in sync.
/// `op()` is the escape hatch for anything not modelled here.
///
/// The canvas holds no document reference and performs no xref work; resources
/// (fonts, XObjects, graphics states) are registered separately.
///
/// Lifetime: the `stream_object` must outlive the canvas, and a canvas must not
/// outlive a `rebind_to_active_revision()` on the object that owns the stream.
/// The canvas is a view with mutable state (the `q` depth), so copying is
/// disabled: two views over the same stream would diverge.
///
/// Every mutating method returns `*this`, so calls chain:
///
/// ```cpp
/// painter.save().translate(72.0, 720.0).path().add_rect(box).fill().restore();
/// ```
///
/// Current scope: graphics state, coordinate transformations, and paths.
/// Text, XObjects, marked content, and resource registration are added on top
/// of `op()` as their own types.
class canvas
{
public:
    explicit canvas(stream_object& stream) noexcept;

    canvas(const canvas&) = delete;
    canvas& operator=(const canvas&) = delete;
    canvas(canvas&&) = delete;
    canvas& operator=(canvas&&) = delete;
    ~canvas() = default;

    /// Returns a fluent view for building and painting a path (§8.5.2).
    [[nodiscard]] path path() noexcept;

    /// Emit `name` with `operands`, in operand-then-operator order.
    ///
    /// @throws parse_exception if an operand is a non-finite real; nothing is
    ///         written in that case.
    canvas& op(std::string_view name, std::vector<object> operands = {});

    /// Emit an already-built instruction.
    canvas& op(const content_operation& operation);

    /// Push a copy of the graphics state (`q`).
    canvas& save();

    /// Pop the graphics state (`Q`).
    ///
    /// @throws logic_exception if called without a matching `save()`, which
    ///         would make the stream unbalanced.
    canvas& restore();

    /// Concatenate `transformation` onto the CTM (`cm`).
    canvas& concat(const geometry::matrix& transformation);

    /// Translate the CTM by `(tx, ty)` (`cm`).
    canvas& translate(double tx, double ty);

    /// Scale the CTM by `(sx, sy)` (`cm`).
    canvas& scale(double sx, double sy);

    /// Rotate the CTM counter-clockwise by `degrees` (`cm`).
    canvas& rotate(double degrees);

    /// Skew the CTM by `x_degrees`/`y_degrees` (`cm`).
    canvas& skew(double x_degrees, double y_degrees);

    /// Set the line width (`w`).
    ///
    /// @throws logic_exception if `width` is negative.
    canvas& set_line_width(double width);

    /// Set the line cap style (`J`).
    canvas& set_line_cap(line_cap cap);

    /// Set the line join style (`j`).
    canvas& set_line_join(line_join join);

    /// Set the miter limit (`M`); must be at least 1.0.
    ///
    /// @throws logic_exception if `limit` is below 1.0.
    canvas& set_miter_limit(double limit);

    /// Set the dash pattern and phase (`d`).
    ///
    /// An empty pattern is a solid line. Entries must be non-negative and at
    /// least one entry must be non-zero.
    ///
    /// @throws logic_exception if the pattern is invalid.
    canvas& set_dash_pattern(std::span<const double> pattern, double phase = 0.0);

    /// Convenience overload for a brace-enclosed dash pattern.
    canvas& set_dash_pattern(std::initializer_list<double> pattern, double phase = 0.0);

    /// Stroke a single segment from `from` to `to` (`m` `l` `S`).
    canvas& stroke_line(geometry::point from, geometry::point to);

    /// Stroke a rectangle (`re` `S`).
    canvas& stroke_rect(const geometry::rect& rect);

    /// Fill a rectangle (`re` `f`/`f*`).
    canvas& fill_rect(const geometry::rect& rect, fill_rule rule = fill_rule::nonzero);

    /// Returns the underlying stream object.
    [[nodiscard]] stream_object& stream() noexcept;

    /// Returns the number of currently open `q` states.
    [[nodiscard]] std::size_t state_depth() const noexcept;

private:
    stream_object& stream_;
    std::size_t state_depth_ = 0;
};
} // namespace ripper::pdf::core
