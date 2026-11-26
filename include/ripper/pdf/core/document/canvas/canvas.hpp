#pragma once

#include "ripper/pdf/core/document/canvas/path/path.hpp"
#include "ripper/pdf/core/document/canvas/text/text.hpp"
#include "ripper/pdf/core/document/catalog/pages/page/content/content_item.hpp"
#include "ripper/pdf/core/document/geometry/matrix.hpp"
#include "ripper/pdf/core/document/geometry/point.hpp"
#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/graphics/color.hpp"
#include "ripper/pdf/core/document/graphics/style.hpp"
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
/// Current scope: graphics state, coordinate transformations, paths, colour,
/// text state and text objects, marked content, and name-addressed XObjects and
/// shadings. Resource registration (`/Font`, `/XObject`, `/ExtGState`, …) is
/// still the caller's job; these methods emit the operators that reference a
/// resource name.
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

    /// Begins a text object (`BT`) and returns a fluent view for it (§9.4).
    ///
    /// @throws logic_exception if a text object is already open.
    [[nodiscard]] text text();

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

    /// Set the rendering intent (`ri`).
    canvas& set_rendering_intent(rendering_intent intent);

    /// Set the flatness tolerance (`i`), which must be in `[0, 100]`.
    ///
    /// @throws logic_exception if `flatness` is outside the spec range.
    canvas& set_flatness(double flatness);

    /// Apply the named graphics state from `/Resources /ExtGState` (`gs`).
    canvas& set_graphics_state(std::string_view name);

    /// Set the nonstroking colour from a device colour value (`g`, `rg`, `k`).
    canvas& set_fill_color(const color& value);

    /// Set the stroking colour from a device colour value (`G`, `RG`, `K`).
    canvas& set_stroke_color(const color& value);

    /// Set the nonstroking colour space by resource name (`cs`).
    canvas& set_fill_color_space(std::string_view space);

    /// Set the stroking colour space by resource name (`CS`).
    canvas& set_stroke_color_space(std::string_view space);

    /// Set nonstroking colour components in the current colour space (`sc`).
    canvas& set_fill_color(std::span<const double> components);

    /// Set stroking colour components in the current colour space (`SC`).
    canvas& set_stroke_color(std::span<const double> components);

    /// Set a nonstroking pattern colour: components plus pattern name (`scn`).
    canvas& set_fill_color(std::span<const double> components, std::string_view pattern);

    /// Set a stroking pattern colour: components plus pattern name (`SCN`).
    canvas& set_stroke_color(std::span<const double> components, std::string_view pattern);

    /// Set the font resource name and size (`Tf`).
    canvas& set_font(std::string_view font_name, double size);

    /// Set character spacing (`Tc`).
    canvas& set_character_spacing(double spacing);

    /// Set word spacing (`Tw`).
    canvas& set_word_spacing(double spacing);

    /// Set horizontal scaling as a percentage (`Tz`).
    canvas& set_horizontal_scaling(double percent);

    /// Set the leading (`TL`).
    canvas& set_leading(double leading);

    /// Set the text rise (`Ts`).
    canvas& set_text_rise(double rise);

    /// Set the text rendering mode (`Tr`).
    canvas& set_text_render_mode(text_render_mode mode);

    /// Draw the named XObject from `/Resources /XObject` (`Do`).
    canvas& draw_xobject(std::string_view name);

    /// Paint the named shading from `/Resources /Shading` (`sh`).
    canvas& paint_shading(std::string_view name);

    /// Write a complete inline image (`BI` … `ID` … `EI`).
    canvas& draw_image(const inline_image& image);

    /// Mark a point in the content stream with a tag (`MP`).
    canvas& mark_point(std::string_view tag);

    /// Mark a point with a tag and an inline property dictionary (`DP`).
    canvas& mark_point(std::string_view tag, const dictionary_object& properties);

    /// Begin a marked-content sequence with a tag (`BMC`).
    canvas& begin_marked_content(std::string_view tag);

    /// Begin a marked-content sequence with a tag and a named property list
    /// from `/Resources /Properties` (`BDC`).
    canvas& begin_marked_content(std::string_view tag, std::string_view properties_name);

    /// Begin a marked-content sequence with a tag and an inline property
    /// dictionary (`BDC`).
    canvas& begin_marked_content(std::string_view tag, const dictionary_object& properties);

    /// End the innermost marked-content sequence (`EMC`).
    ///
    /// @throws logic_exception if no marked-content sequence is open.
    canvas& end_marked_content();

    /// Begin a compatibility section (`BX`).
    canvas& begin_compatibility();

    /// End the innermost compatibility section (`EX`).
    ///
    /// @throws logic_exception if no compatibility section is open.
    canvas& end_compatibility();

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

    /// Returns `true` while a text object opened by `text()` is still open.
    [[nodiscard]] bool in_text() const noexcept;

    /// Returns the number of currently open marked-content sequences.
    [[nodiscard]] std::size_t marked_content_depth() const noexcept;

    /// Returns the number of currently open compatibility sections.
    [[nodiscard]] std::size_t compatibility_depth() const noexcept;

private:
    friend class text;

    stream_object& stream_;
    std::size_t state_depth_ = 0;
    std::size_t marked_content_depth_ = 0;
    std::size_t compatibility_depth_ = 0;
    bool in_text_ = false;
};
} // namespace ripper::pdf::core
