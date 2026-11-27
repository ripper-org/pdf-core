#include "ripper/pdf/core/document/font/font.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
[[nodiscard]] bool is_latin_standard_font(standard_font value) noexcept
{
    return value != standard_font::symbol && value != standard_font::zapf_dingbats;
}
} // namespace

std::string_view standard_font_name(standard_font value) noexcept
{
    switch (value)
    {
        case standard_font::helvetica:
            return "Helvetica";
        case standard_font::helvetica_bold:
            return "Helvetica-Bold";
        case standard_font::helvetica_oblique:
            return "Helvetica-Oblique";
        case standard_font::helvetica_bold_oblique:
            return "Helvetica-BoldOblique";
        case standard_font::times_roman:
            return "Times-Roman";
        case standard_font::times_bold:
            return "Times-Bold";
        case standard_font::times_italic:
            return "Times-Italic";
        case standard_font::times_bold_italic:
            return "Times-BoldItalic";
        case standard_font::courier:
            return "Courier";
        case standard_font::courier_bold:
            return "Courier-Bold";
        case standard_font::courier_oblique:
            return "Courier-Oblique";
        case standard_font::courier_bold_oblique:
            return "Courier-BoldOblique";
        case standard_font::symbol:
            return "Symbol";
        case standard_font::zapf_dingbats:
            return "ZapfDingbats";
    }

    return {};
}

font::font(indirect_object& obj) : indirect_object_view(obj)
{
    if (obj.content().as_dictionary() == nullptr)
        throw logic_exception{"Font object must be a dictionary"};
}

font font::create_standard(document& doc, standard_font value)
{
    dictionary_object dict;
    dict.set("Type", object{name_object{"Font"}});
    dict.set("Subtype", object{name_object{"Type1"}});
    dict.set("BaseFont", object{name_object{std::string{standard_font_name(value)}}});

    if (is_latin_standard_font(value))
        dict.set("Encoding", object{name_object{"WinAnsiEncoding"}});

    auto& obj = object_manager::allocate_object(doc, object{std::move(dict)});
    return font{obj};
}

const dictionary_object& font::dictionary() const
{
    const auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Font object must be a dictionary"};

    return *dict;
}

dictionary_object& font::dictionary()
{
    auto* dict = obj().content().as_dictionary();
    if (dict == nullptr)
        throw logic_exception{"Font object must be a dictionary"};

    return *dict;
}

std::string_view font::subtype() const
{
    const auto* subtype_entry = dictionary().get_name("Subtype");

    return subtype_entry == nullptr ? std::string_view{} : std::string_view{subtype_entry->value};
}

std::string_view font::base_font() const
{
    const auto* base_font_entry = dictionary().get_name("BaseFont");

    return base_font_entry == nullptr ? std::string_view{}
                                      : std::string_view{base_font_entry->value};
}

bool font::is_composite() const
{
    return subtype() == "Type0";
}

std::string_view font::encoding() const
{
    const auto* encoding_entry = dictionary().get_name("Encoding");

    return encoding_entry == nullptr ? std::string_view{} : std::string_view{encoding_entry->value};
}
} // namespace ripper::pdf::core
