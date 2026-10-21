#pragma once

#include <string_view>

namespace ripper::pdf::core::content_operator
{
// General graphics state (§8.4)
inline constexpr std::string_view w{"w"};   ///< Line width
inline constexpr std::string_view J{"J"};   ///< Line cap style
inline constexpr std::string_view j{"j"};   ///< Line join style
inline constexpr std::string_view M{"M"};   ///< Miter limit
inline constexpr std::string_view d{"d"};   ///< Line dash pattern
inline constexpr std::string_view ri{"ri"}; ///< Rendering intent
inline constexpr std::string_view i{"i"};   ///< Flatness tolerance
inline constexpr std::string_view gs{"gs"}; ///< Extended graphics state
inline constexpr std::string_view q{"q"};   ///< Save graphics state
inline constexpr std::string_view Q{"Q"};   ///< Restore graphics state
inline constexpr std::string_view cm{"cm"}; ///< Modify current transformation matrix

// Path construction (§8.5.2)
inline constexpr std::string_view m{"m"};   ///< Move to
inline constexpr std::string_view l{"l"};   ///< Line to
inline constexpr std::string_view c{"c"};   ///< Cubic Bézier curve
inline constexpr std::string_view v{"v"};   ///< Cubic Bézier curve (initial point replicated)
inline constexpr std::string_view y{"y"};   ///< Cubic Bézier curve (final point replicated)
inline constexpr std::string_view h{"h"};   ///< Close subpath
inline constexpr std::string_view re{"re"}; ///< Rectangle

// Path painting (§8.5.3)
inline constexpr std::string_view S{"S"};       ///< Stroke
inline constexpr std::string_view s{"s"};       ///< Close and stroke
inline constexpr std::string_view f{"f"};       ///< Fill (nonzero winding)
inline constexpr std::string_view F{"F"};       ///< Fill (nonzero winding, deprecated alias)
inline constexpr std::string_view f_star{"f*"}; ///< Fill (even-odd)
inline constexpr std::string_view B{"B"};       ///< Fill and stroke (nonzero winding)
inline constexpr std::string_view B_star{"B*"}; ///< Fill and stroke (even-odd)
inline constexpr std::string_view b{"b"};       ///< Close, fill, and stroke (nonzero winding)
inline constexpr std::string_view b_star{"b*"}; ///< Close, fill, and stroke (even-odd)
inline constexpr std::string_view n{"n"};       ///< End path without painting

// Clipping (§8.5.4)
inline constexpr std::string_view W{"W"};       ///< Clip (nonzero winding)
inline constexpr std::string_view W_star{"W*"}; ///< Clip (even-odd)

// Color (§8.6.8)
inline constexpr std::string_view cs{"cs"};   ///< Set stroking color space
inline constexpr std::string_view CS{"CS"};   ///< Set nonstroking color space
inline constexpr std::string_view sc{"sc"};   ///< Set stroking color
inline constexpr std::string_view SC{"SC"};   ///< Set nonstroking color
inline constexpr std::string_view scn{"scn"}; ///< Set stroking color (extended)
inline constexpr std::string_view SCN{"SCN"}; ///< Set nonstroking color (extended)
inline constexpr std::string_view g{"g"};     ///< Set stroking gray
inline constexpr std::string_view G{"G"};     ///< Set nonstroking gray
inline constexpr std::string_view rg{"rg"};   ///< Set stroking RGB
inline constexpr std::string_view RG{"RG"};   ///< Set nonstroking RGB
inline constexpr std::string_view k{"k"};     ///< Set stroking CMYK
inline constexpr std::string_view K{"K"};     ///< Set nonstroking CMYK

// Text state (§9.3)
inline constexpr std::string_view Tc{"Tc"}; ///< Character spacing
inline constexpr std::string_view Tw{"Tw"}; ///< Word spacing
inline constexpr std::string_view Tz{"Tz"}; ///< Horizontal scaling
inline constexpr std::string_view TL{"TL"}; ///< Text leading
inline constexpr std::string_view Tf{"Tf"}; ///< Font and font size
inline constexpr std::string_view Tr{"Tr"}; ///< Text rendering mode
inline constexpr std::string_view Ts{"Ts"}; ///< Text rise

// Text object and positioning (§9.4)
inline constexpr std::string_view BT{"BT"};           ///< Begin text object
inline constexpr std::string_view ET{"ET"};           ///< End text object
inline constexpr std::string_view Td{"Td"};           ///< Move text position
inline constexpr std::string_view TD{"TD"};           ///< Move text position and set leading
inline constexpr std::string_view Tm{"Tm"};           ///< Set text matrix
inline constexpr std::string_view T_star{"T*"};       ///< Move to next line
inline constexpr std::string_view Tj{"Tj"};           ///< Show text
inline constexpr std::string_view TJ{"TJ"};           ///< Show text array
inline constexpr std::string_view single_quote{"'"};  ///< Next line and show text
inline constexpr std::string_view double_quote{"\""}; ///< Set spacing, next line, and show text

// XObjects (§8.10)
inline constexpr std::string_view Do{"Do"}; ///< Draw XObject

// Inline images (§8.9.7)
inline constexpr std::string_view BI{"BI"}; ///< Begin inline image
inline constexpr std::string_view ID{"ID"}; ///< Inline image data
inline constexpr std::string_view EI{"EI"}; ///< End inline image

// Marked content (§14.6)
inline constexpr std::string_view MP{"MP"};   ///< Marked point
inline constexpr std::string_view DP{"DP"};   ///< Marked point with property list
inline constexpr std::string_view BMC{"BMC"}; ///< Begin marked content
inline constexpr std::string_view BDC{"BDC"}; ///< Begin marked content with property list
inline constexpr std::string_view EMC{"EMC"}; ///< End marked content

// Compatibility (§14.7)
inline constexpr std::string_view BX{"BX"}; ///< Begin compatibility section
inline constexpr std::string_view EX{"EX"}; ///< End compatibility section

// Shading (§8.7)
inline constexpr std::string_view sh{"sh"}; ///< Paint shading

// Type 3 font metrics (§9.6.6)
inline constexpr std::string_view d0{"d0"}; ///< Set glyph width
inline constexpr std::string_view d1{"d1"}; ///< Set glyph width and bounding box
} // namespace ripper::pdf::core::content_operator
