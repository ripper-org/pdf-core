#pragma once

#include "ripper/pdf/core/document/geometry/matrix.hpp"
#include "ripper/pdf/core/document/graphics/style.hpp"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace ripper::pdf::core::graphics
{
class brush;

/// One entry of a `TJ` array: either a string to show or a displacement in
/// thousandths of a unit of text space (ISO 32000-1 §9.4.3).
using text_show_item = std::variant<std::string, double>;

/// Fluent view over a text object (ISO 32000-1 §9.4).
///
/// Obtained from `brush::text()`, which emits `BT`; `end()` emits `ET` and
/// returns the owning brush. Text objects cannot nest, so opening a second one
/// before ending the first raises `logic_exception`.
///
/// ```cpp
/// painter.text()
///        .set_font("F1", 12.0)
///        .move_by(72.0, 720.0)
///        .show("Hello")
///        .next_line()
///        .show("World")
///        .end();
/// ```
///
/// Strings passed to `show()` are written verbatim as PDF literal strings, so
/// they must already be encoded for the current font (WinAnsi, MacRoman, an
/// identity CMap for a composite font, …). No Unicode conversion is performed.
class text
{
public:
    text(const text&) = delete;
    text& operator=(const text&) = delete;
    text(text&&) = delete;
    text& operator=(text&&) = delete;
    ~text() = default;

    /// Set the font resource name and size (`Tf`).
    text& set_font(std::string_view font_name, double size);

    /// Set character spacing (`Tc`).
    text& set_character_spacing(double spacing);

    /// Set word spacing (`Tw`).
    text& set_word_spacing(double spacing);

    /// Set horizontal scaling as a percentage (`Tz`).
    text& set_horizontal_scaling(double percent);

    /// Set the leading (`TL`).
    text& set_leading(double leading);

    /// Set the text rise (`Ts`).
    text& set_text_rise(double rise);

    /// Set the text rendering mode (`Tr`).
    text& set_text_render_mode(text_render_mode mode);

    /// Move the text position by `(tx, ty)` (`Td`).
    text& move_by(double tx, double ty);

    /// Move the text position and set the leading (`TD`).
    text& move_by_and_set_leading(double tx, double ty);

    /// Move to the start of the next line (`T*`).
    text& next_line();

    /// Set the text matrix and line matrix (`Tm`).
    text& set_matrix(const geometry::matrix& transformation);

    /// Show an already font-encoded string (`Tj`).
    text& show(std::string_view encoded_text);

    /// Show a `TJ` array (`TJ`).
    text& show(std::span<const text_show_item> items);

    /// Convenience overload for a brace-enclosed `TJ` array.
    text& show(std::initializer_list<text_show_item> items);

    /// Move to the next line and show `encoded_text` (`'`).
    text& show_next_line(std::string_view encoded_text);

    /// Set word and character spacing, move to the next line, and show
    /// `encoded_text` (`"`).
    text& show_next_line(double word_spacing, double character_spacing,
                         std::string_view encoded_text);

    /// End the text object (`ET`) and return the owning brush.
    ///
    /// @throws logic_exception if the text object was already ended.
    brush& end();

private:
    friend class brush;

    explicit text(brush& target) noexcept : brush_{&target} {}

    /// @throws logic_exception if the text object has already been ended.
    void ensure_active() const;

    brush* brush_;
    bool ended_ = false;
};
} // namespace ripper::pdf::core::graphics
