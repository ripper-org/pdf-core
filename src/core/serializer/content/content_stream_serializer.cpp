#include "ripper/pdf/core/serializer/content/content_stream_serializer.hpp"

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/serializer/object/default_object_serializer.hpp"
#include "ripper/pdf/core/util/byte.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <variant>

namespace ripper::pdf::core
{
namespace
{
void append_operation(std::vector<std::byte>& out, const content_operation& operation,
                      const default_object_serializer& serializer)
{
    for (const auto& operand : operation.operands)
    {
        byte::append_bytes(out, serializer.serialize(operand));
        byte::append_bytes(out, ' ');
    }

    byte::append_bytes(out, operation.operator_name);
    byte::append_bytes(out, '\n');
}

void append_inline_image(std::vector<std::byte>& out, const inline_image& image,
                         const default_object_serializer& serializer)
{
    byte::append_bytes(out, "BI\n");

    for (const auto& [key, value] : image.dictionary.entries())
    {
        byte::append_bytes(out, '/');
        byte::append_bytes(out, text::escape_name(key));
        byte::append_bytes(out, ' ');
        byte::append_bytes(out, serializer.serialize(value));
        byte::append_bytes(out, '\n');
    }

    // One white-space separator after `ID` and one before `EI`; neither belongs
    // to the data, so the byte payload survives a parser round-trip unchanged.
    byte::append_bytes(out, "ID ");
    out.append_range(image.data);
    byte::append_bytes(out, "\nEI\n");
}

void append_item(std::vector<std::byte>& out, const content_item& item,
                 const default_object_serializer& serializer)
{
    if (const auto* operation = std::get_if<content_operation>(&item))
    {
        append_operation(out, *operation, serializer);
        return;
    }

    append_inline_image(out, std::get<inline_image>(item), serializer);
}
} // namespace

std::vector<std::byte> content_stream_serializer::serialize(const content_operation& operation)
{
    std::vector<std::byte> out;
    append_operation(out, operation, default_object_serializer{});
    return out;
}

std::vector<std::byte> content_stream_serializer::serialize(const inline_image& image)
{
    std::vector<std::byte> out;
    append_inline_image(out, image, default_object_serializer{});
    return out;
}

std::vector<std::byte> content_stream_serializer::serialize(const content_item& item)
{
    std::vector<std::byte> out;
    append_item(out, item, default_object_serializer{});
    return out;
}

std::vector<std::byte> content_stream_serializer::serialize(std::span<const content_item> items)
{
    const default_object_serializer object_serializer;
    std::vector<std::byte> out;

    for (const auto& item : items)
        append_item(out, item, object_serializer);

    return out;
}

std::vector<std::byte> content_stream_serializer::serialize(const instruction_stream& stream)
{
    const default_object_serializer object_serializer;
    std::vector<std::byte> out;

    for (const auto& item : stream.operations())
        append_item(out, item, object_serializer);

    return out;
}
} // namespace ripper::pdf::core
