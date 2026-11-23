#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_item.hpp"
#include "ripper/pdf/core/document/geometry/matrix.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstddef>
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
///
/// Current scope: graphics state and coordinate transformations only. Painting,
/// text, XObjects, marked content, and resource registration are added on top
/// of `op()` as their own types.
class canvas
{
public:
    explicit canvas(stream_object& stream) noexcept;

    /// Emit `name` with `operands`, in operand-then-operator order.
    ///
    /// @throws parse_exception if an operand is a non-finite real; nothing is
    ///         written in that case.
    void op(std::string_view name, std::vector<object> operands = {});

    /// Emit an already-built instruction.
    void op(const content_operation& operation);

    /// Push a copy of the graphics state (`q`).
    void save();

    /// Pop the graphics state (`Q`).
    ///
    /// @throws logic_exception if called without a matching `save()`, which
    ///         would make the stream unbalanced.
    void restore();

    /// Concatenate `transformation` onto the CTM (`cm`).
    void concat(const geometry::matrix& transformation);

    /// Translate the CTM by `(tx, ty)` (`cm`).
    void translate(double tx, double ty);

    /// Scale the CTM by `(sx, sy)` (`cm`).
    void scale(double sx, double sy);

    /// Rotate the CTM counter-clockwise by `degrees` (`cm`).
    void rotate(double degrees);

    /// Skew the CTM by `x_degrees`/`y_degrees` (`cm`).
    void skew(double x_degrees, double y_degrees);

    /// Returns the underlying stream object.
    [[nodiscard]] stream_object& stream() noexcept;

    /// Returns the number of currently open `q` states.
    [[nodiscard]] std::size_t state_depth() const noexcept;

private:
    stream_object& stream_;
    std::size_t state_depth_ = 0;
};
} // namespace ripper::pdf::core
