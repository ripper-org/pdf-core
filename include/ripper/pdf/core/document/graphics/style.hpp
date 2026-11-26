#pragma once

#include <cstdint>

namespace ripper::pdf::core::graphics
{
/// Winding rule used when filling or clipping a path (ISO 32000-1 §8.5.3.1).
enum class fill_rule
{
    nonzero,
    even_odd
};

/// Line cap style, the operand of `J` (ISO 32000-1 §8.4.3.3, Table 52).
enum class line_cap
{
    butt = 0,
    round = 1,
    square = 2
};

/// Line join style, the operand of `j` (ISO 32000-1 §8.4.3.4, Table 53).
enum class line_join
{
    miter = 0,
    round = 1,
    bevel = 2
};

/// One of the eight path-painting operators (ISO 32000-1 §8.5.3, Table 60).
enum class paint_style
{
    stroke,
    close_stroke,
    fill,
    fill_even_odd,
    fill_stroke,
    fill_stroke_even_odd,
    close_fill_stroke,
    close_fill_stroke_even_odd
};

/// Text rendering mode, the operand of `Tr` (ISO 32000-1 §9.3.6, Table 106).
enum class text_render_mode : std::uint8_t
{
    fill = 0,             ///< Fill text.
    stroke = 1,           ///< Stroke text.
    fill_stroke = 2,      ///< Fill, then stroke text.
    invisible = 3,        ///< Neither fill nor stroke (invisible but selectable).
    fill_clip = 4,        ///< Fill text and add to the path for clipping.
    stroke_clip = 5,      ///< Stroke text and add to the path for clipping.
    fill_stroke_clip = 6, ///< Fill and stroke text, then add to the clip path.
    clip = 7              ///< Add text to the path for clipping only.
};

/// Rendering intent, the operand of `ri` (ISO 32000-1 §8.6.5.8, Table 71).
enum class rendering_intent
{
    absolute_colorimetric,
    relative_colorimetric,
    saturation,
    perceptual
};
} // namespace ripper::pdf::core::graphics
