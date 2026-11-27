#pragma once

#include "ripper/pdf/core/document/object/helpers/indirect_object_view.hpp"

#include <string_view>

namespace ripper::pdf::core
{
class dictionary_object;
class document;
class indirect_object;

/// Type 1 fonts every conforming reader supplies, so they never need to be
/// embedded (ISO 32000-1 §9.6.2.2, Table 112).
enum class standard_font
{
    helvetica,
    helvetica_bold,
    helvetica_oblique,
    helvetica_bold_oblique,
    times_roman,
    times_bold,
    times_italic,
    times_bold_italic,
    courier,
    courier_bold,
    courier_oblique,
    courier_bold_oblique,
    symbol,
    zapf_dingbats
};

/// Returns the `/BaseFont` name of `value`, e.g. `"Helvetica-Bold"`.
[[nodiscard]] std::string_view standard_font_name(standard_font value) noexcept;

/// Typed view over a font dictionary (ISO 32000-1 §9.6).
///
/// A font is an indirect object whose content is a dictionary with at least
/// `/Type /Font` and a `/Subtype`. This view validates only the shape needed to
/// hand the font to a content stream; it does not load or embed font programs.
///
/// Only standard-14 Type 1 fonts can be created so far, through
/// `create_standard()`. Parsed fonts — simple or composite — can be wrapped by
/// resolving their indirect object and constructing a `font` from it.
///
/// The owning `indirect_object` must outlive the view.
class font : public indirect_object_view
{
public:
    /// Wraps `obj` as a font.
    ///
    /// @throws logic_exception if `obj` does not hold a dictionary.
    explicit font(indirect_object& obj);

    /// Allocates a new standard-14 Type 1 font and commits it to the active
    /// revision.
    ///
    /// The generated dictionary contains `/Type /Font`, `/Subtype /Type1`,
    /// `/BaseFont`, and `/Encoding /WinAnsiEncoding` for the Latin text fonts.
    /// `Symbol` and `ZapfDingbats` keep their built-in encoding, so no
    /// `/Encoding` entry is written for them.
    ///
    /// Standard-14 fonts are not embedded: the reader is expected to supply the
    /// font program.
    ///
    /// @throws logic_exception if the commit fails.
    [[nodiscard]] static font create_standard(document& doc, standard_font value);

    /// Returns the font dictionary.
    [[nodiscard]] const dictionary_object& dictionary() const;
    [[nodiscard]] dictionary_object& dictionary();

    /// Returns `/Subtype` (for example `"Type1"`, `"TrueType"`, `"Type0"`), or
    /// an empty view when the entry is absent.
    [[nodiscard]] std::string_view subtype() const;

    /// Returns `/BaseFont`, or an empty view when the entry is absent.
    [[nodiscard]] std::string_view base_font() const;

    /// Returns `true` when `/Subtype` is `/Type0` (a composite font).
    [[nodiscard]] bool is_composite() const;

    /// Returns the `/Encoding` name for a simple font, or an empty view when
    /// the entry is absent, is a dictionary (a custom encoding), or is not a
    /// name.
    [[nodiscard]] std::string_view encoding() const;
};
} // namespace ripper::pdf::core
