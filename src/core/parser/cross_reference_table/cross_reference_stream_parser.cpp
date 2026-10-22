#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_stream_parser.hpp"

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/checked.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace ripper::pdf::core
{

cross_reference_section cross_reference_stream_parser::parse(const stream_object& stream_obj)
{
    const auto& dict = stream_obj.dictionary();
    const auto content = stream_obj.raw();

    // PDF 32000-1 §7.5.8: an xref stream must be /Type /XRef. Rejecting anything
    // else prevents a lookalike stream from being misinterpreted as a table.
    const auto* type_ptr = dict.get_name("Type");
    if (type_ptr == nullptr || type_ptr->value != "XRef")
        throw parse_exception{"Object is not an xref stream (/Type /XRef required)"};

    const auto* size_ptr = dict.get_number("Size");
    if (size_ptr == nullptr || !size_ptr->is_integer() || size_ptr->as_integer() <= 0)
        throw parse_exception{"Xref stream missing required /Size"};

    const auto size =
        checked::to_unsigned<std::uint32_t>(size_ptr->as_integer(), "Xref stream /Size");
    const auto widths = parse_w(stream_obj);
    const auto ranges = parse_index(stream_obj, size);

    const auto entry_size =
        checked::add(checked::add(static_cast<std::size_t>(widths.w0),
                                  static_cast<std::size_t>(widths.w1), "Xref stream /W"),
                     static_cast<std::size_t>(widths.w2), "Xref stream /W");

    if (entry_size == 0)
        throw parse_exception{"Xref stream entry size is zero"};

    cross_reference_section section{std::deque<cross_reference_subsection>{}};
    std::size_t byte_offset = 0;

    for (const auto& range : ranges)
    {
        const auto range_end = static_cast<std::uint64_t>(range.first) + range.count;
        if (range_end > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U)
            throw parse_exception{"Xref stream /Index range overflows the object number space"};

        for (std::uint32_t i = 0; i < range.count; ++i)
        {
            if (byte_offset > content.size() || entry_size > content.size() - byte_offset)
                throw parse_exception{"Xref stream data truncated"};

            const auto entry_data = content.subspan(byte_offset, entry_size);

            const auto type_raw = widths.w0 == 0 ? 1 : read_field(entry_data, 0, widths.w0);
            const auto field1 = read_field(entry_data, widths.w0, widths.w1);
            const auto field2 =
                widths.w2 > 0 ? read_field(entry_data, widths.w0 + widths.w1, widths.w2) : 0;

            const auto obj_num = range.first + i;

            if (type_raw > 2)
            {
                // Per §7.5.8, entries with an unknown type are treated as null.
                // Record a terminal free entry without trusting the raw fields.
                section.add_entry(
                    cross_reference_entry::make_free(indirect_reference{obj_num, 0}, 0, 0));
                byte_offset += entry_size;
                continue;
            }

            const auto type = static_cast<xref_entry_type>(type_raw);

            switch (type)
            {
                case xref_entry_type::free:
                {
                    section.add_entry(cross_reference_entry::make_free(
                        indirect_reference{obj_num, 0},
                        checked::to_unsigned<std::uint32_t>(field1, "Xref stream free-list entry"),
                        checked::to_unsigned<std::uint16_t>(field2,
                                                            "Xref stream free generation")));
                    break;
                }
                case xref_entry_type::uncompressed:
                {
                    section.add_entry(cross_reference_entry{
                        indirect_reference{obj_num, checked::to_unsigned<std::uint16_t>(
                                                        field2, "Xref stream generation number")},
                        field1, true});
                    break;
                }
                case xref_entry_type::compressed:
                {
                    section.add_entry(cross_reference_entry{
                        indirect_reference{obj_num, 0},
                        checked::to_unsigned<std::uint32_t>(field1,
                                                            "Xref stream object stream number"),
                        checked::to_unsigned<std::uint32_t>(field2,
                                                            "Xref stream object stream index")});
                    break;
                }
                default:
                    break;
            }

            byte_offset += entry_size;
        }
    }

    return section;
}

cross_reference_stream_parser::column_widths
cross_reference_stream_parser::parse_w(const stream_object& stream_obj)
{
    const auto* w = stream_obj.dictionary().get_array("W");
    if (w == nullptr || w->size() != 3)
        throw parse_exception{"Xref stream /W array must contain exactly three integers"};

    column_widths result;

    const auto width_at = [&](std::size_t index, const char* name) -> std::uint32_t
    {
        const auto* number = w->at(index).as_number();
        if (number == nullptr || !number->is_integer())
            throw parse_exception{std::string{"Xref stream /W "} + name + " must be an integer"};

        const auto value = number->as_integer();
        if (value < 0)
            throw parse_exception{std::string{"Xref stream /W "} + name + " must not be negative"};

        if (value > 8)
            throw parse_exception{std::string{"Xref stream /W "} + name + " exceeds 8 bytes"};

        return static_cast<std::uint32_t>(value);
    };

    result.w0 = width_at(0, "type width");
    result.w1 = width_at(1, "field 1 width");
    result.w2 = width_at(2, "field 2 width");

    return result;
}

std::vector<cross_reference_stream_parser::subsection_range>
cross_reference_stream_parser::parse_index(const stream_object& stream_obj, std::uint32_t size)
{
    const auto* index = stream_obj.dictionary().get_array("Index");

    if (index == nullptr)
    {
        return {{0, size}};
    }

    if (index->size() % 2 != 0)
        throw parse_exception{"/Index array must have an even number of elements"};

    std::vector<subsection_range> ranges;
    ranges.reserve(index->size() / 2);

    for (std::size_t i = 0; i < index->size(); i += 2)
    {
        const auto* first = index->at(i).as_number();
        const auto* count = index->at(i + 1).as_number();

        if (first == nullptr || count == nullptr || !first->is_integer() || !count->is_integer())
            throw parse_exception{"/Index array entries must be integers"};

        if (first->as_integer() < 0 || count->as_integer() < 0)
            throw parse_exception{"/Index array entries must not be negative"};

        ranges.push_back(
            {checked::to_unsigned<std::uint32_t>(first->as_integer(), "/Index first object number"),
             checked::to_unsigned<std::uint32_t>(count->as_integer(), "/Index object count")});
    }

    return ranges;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
std::uint64_t cross_reference_stream_parser::read_field(std::span<const std::byte> data,
                                                        std::size_t offset, std::uint32_t width)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    checked::require_range(offset, width, data.size(), "Xref stream field");

    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i)
    {
        value = (value << 8) |
                static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(data[offset + i]));
    }
    return value;
}

} // namespace ripper::pdf::core
