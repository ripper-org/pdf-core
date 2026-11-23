#pragma once

namespace ripper::pdf::core
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
} // namespace ripper::pdf::core
