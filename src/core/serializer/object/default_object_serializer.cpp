#include "ripper/pdf/core/serializer/object/default_object_serializer.hpp"

#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/filter/filter_manager.hpp"
#include "ripper/pdf/core/util/byte.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{
std::vector<std::byte> default_object_serializer::serialize(const object& obj) const
{
    if (obj.is_null())
        return serialize_null();

    if (const auto* value = obj.as_boolean())
        return serialize_bool(value->value);

    if (const auto* value = obj.as_number())
    {
        if (value->is_real())
            return serialize_real(value->as_real());
        return serialize_integer(value->as_integer());
    }

    if (const auto* value = obj.as_string())
        return serialize_string(value->as_string());

    if (const auto* value = obj.as_name())
        return serialize_name(*value);

    if (const auto* value = obj.as_indirect_reference())
        return serialize_indirect_reference(*value);

    if (const auto* value = obj.as_array())
        return serialize_array(*value);

    // Streams must be checked before dictionaries: `as_dictionary()` deliberately
    // reports a stream's content dictionary.
    if (const auto* value = obj.as_stream())
        return serialize_stream_object(*value);

    if (const auto* value = obj.as_dictionary())
        return serialize_dictionary(*value);

    // A moved-from dictionary or stream alternative holds no value.
    return {};
}

std::vector<std::byte> default_object_serializer::serialize_bool(bool value) const
{
    return byte::to_bytes(value ? std::string_view{"true"} : std::string_view{"false"});
}

std::vector<std::byte> default_object_serializer::serialize_integer(std::int64_t value) const
{
    return byte::to_bytes(std::to_string(value));
}

std::vector<std::byte> default_object_serializer::serialize_real(double value) const
{
    if (!std::isfinite(value))
        throw logic_exception{"Cannot serialize non-finite real value"};

    std::array<char, 64> buf{};

    /// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);

    if (ec != std::errc{})
        throw logic_exception{"Failed to serialize real number"};

    std::string s(buf.data(), ptr - buf.data());
    if (s.find('.') == std::string::npos)
    {
        const auto epos = s.find('e');
        if (epos == std::string::npos)
            s += ".0";
        else
            s.insert(epos, ".0");
    }

    return byte::to_bytes(s);
}

std::vector<std::byte> default_object_serializer::serialize_string(const std::string& value) const
{
    const auto escaped = text::escape_literal_string(value);

    std::vector<std::byte> out;
    byte::append_bytes(out, '(');
    byte::append_bytes(out, escaped);
    byte::append_bytes(out, ')');

    return out;
}

std::vector<std::byte> default_object_serializer::serialize_name(const name_object& value) const
{
    const auto escaped = text::escape_name(value.value);

    std::vector<std::byte> out;
    byte::append_bytes(out, '/');
    byte::append_bytes(out, escaped);

    return out;
}

std::vector<std::byte>
default_object_serializer::serialize_indirect_reference(const indirect_reference& value) const
{
    std::vector<std::byte> out;
    byte::append_bytes(out, std::to_string(value.object_number()));
    byte::append_bytes(out, ' ');
    byte::append_bytes(out, std::to_string(value.generation()));
    byte::append_bytes(out, ' ');
    byte::append_bytes(out, 'R');

    return out;
}

std::vector<std::byte> default_object_serializer::serialize_array(const array_object& value) const
{
    std::vector<std::byte> out;
    byte::append_bytes(out, '[');
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            byte::append_bytes(out, ' ');
        byte::append_bytes(out, serialize(value[i]));
    }
    byte::append_bytes(out, ']');

    return out;
}

std::vector<std::byte>
default_object_serializer::serialize_dictionary(const dictionary_object& value) const
{
    std::vector<std::byte> out;
    byte::append_bytes(out, '<');
    byte::append_bytes(out, '<');
    byte::append_bytes(out, object_break_character_);
    for (const auto& [key, val] : value.entries())
    {
        byte::append_bytes(out, serialize_name(name_object{key}));
        byte::append_bytes(out, ' ');
        byte::append_bytes(out, serialize(val));
        byte::append_bytes(out, object_break_character_);
    }
    byte::append_bytes(out, '>');
    byte::append_bytes(out, '>');

    return out;
}

std::vector<std::byte>
default_object_serializer::serialize_stream_object(const stream_object& stream_obj) const
{
    std::vector<std::byte> out;

    const auto dict = stream_obj.dictionary();
    const auto stream = stream_obj.stream();

    if (stream_obj.is_decoded())
    {
        auto encoded = filter_manager::encode(dict, stream.data());

        dictionary_object encoded_dict = dict;
        encoded_dict.set("Length", object{static_cast<std::int64_t>(encoded.size())});

        byte::append_bytes(out, serialize_dictionary(encoded_dict));
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, "stream");
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, encoded);
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, "endstream");
    }
    else
    {
        byte::append_bytes(out, serialize_dictionary(dict));
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, "stream");
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, stream.data());
        byte::append_bytes(out, line_break_character_);
        byte::append_bytes(out, "endstream");
    }

    return out;
}
} // namespace ripper::pdf::core
