#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_stream_parser.hpp"

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/byte_count.hpp"
#include "ripper/pdf/core/util/byte_offset.hpp"
#include "ripper/pdf/core/util/byte_range.hpp"
#include "ripper/pdf/core/util/checked_numeric.hpp"

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
        checked_numeric::narrow<std::uint32_t>(size_ptr->as_integer(), "Xref stream /Size");
    const auto widths = parse_w(stream_obj);
    const auto ranges = parse_index(stream_obj, size);

    const auto entry_size = byte_count::from_integer(widths.w0.value())
                                .plus(byte_count::from_integer(widths.w1.value()))
                                .plus(byte_count::from_integer(widths.w2.value()));

    if (entry_size.value() == 0)
        throw parse_exception{"Xref stream entry size is zero"};

    const auto field1_offset = byte_offset::from_integer(widths.w0.value());
    const auto field2_offset =
        field1_offset.advanced_by(byte_count::from_integer(widths.w1.value()));

    cross_reference_section section{std::deque<cross_reference_subsection>{}};
    auto byte_position = byte_offset::zero();

    for (const auto& range : ranges)
    {
        const auto range_end = static_cast<std::uint64_t>(range.first) + range.count;
        if (range_end > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U)
            throw parse_exception{"Xref stream /Index range overflows the object number space"};

        for (std::uint32_t i = 0; i < range.count; ++i)
        {
            const auto entry_data =
                checked_subspan(content, byte_position, entry_size, "Xref stream entry");

            const auto type_raw =
                widths.w0.value() == 0 ? 1 : read_field(entry_data, byte_offset::zero(), widths.w0);
            const auto field1 = read_field(entry_data, field1_offset, widths.w1);
            const auto field2 =
                widths.w2.value() > 0 ? read_field(entry_data, field2_offset, widths.w2) : 0;

            const auto obj_num = range.first + i;

            if (type_raw > 2)
            {
                // Per §7.5.8, entries with an unknown type are treated as null.
                // Record a terminal free entry without trusting the raw fields.
                section.add_entry(
                    cross_reference_entry::make_free(indirect_reference{obj_num, 0}, 0, 0));
                byte_position = byte_position.advanced_by(entry_size);
                continue;
            }

            const auto type = static_cast<xref_entry_type>(type_raw);

            switch (type)
            {
                case xref_entry_type::free:
                {
                    section.add_entry(cross_reference_entry::make_free(
                        indirect_reference{obj_num, 0},
                        checked_numeric::narrow<std::uint32_t>(field1,
                                                               "Xref stream free-list entry"),
                        checked_numeric::narrow<std::uint16_t>(field2,
                                                               "Xref stream free generation")));
                    break;
                }
                case xref_entry_type::uncompressed:
                {
                    section.add_entry(cross_reference_entry{
                        indirect_reference{obj_num, checked_numeric::narrow<std::uint16_t>(
                                                        field2, "Xref stream generation number")},
                        field1, true});
                    break;
                }
                case xref_entry_type::compressed:
                {
                    section.add_entry(cross_reference_entry{
                        indirect_reference{obj_num, 0},
                        checked_numeric::narrow<std::uint32_t>(field1,
                                                               "Xref stream object stream number"),
                        checked_numeric::narrow<std::uint32_t>(field2,
                                                               "Xref stream object stream index")});
                    break;
                }
                default:
                    break;
            }

            byte_position = byte_position.advanced_by(entry_size);
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

    const auto width_at = [&](std::size_t index, const char* name) -> xref_field_width
    {
        const auto* number = w->at(index).as_number();
        if (number == nullptr || !number->is_integer())
            throw parse_exception{std::string{"Xref stream /W "} + name + " must be an integer"};

        const auto value = number->as_integer();
        if (value < 0)
            throw parse_exception{std::string{"Xref stream /W "} + name + " must not be negative"};

        if (value > xref_field_width::max_width)
            throw parse_exception{std::string{"Xref stream /W "} + name + " exceeds 8 bytes"};

        return xref_field_width::from_integer(value);
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
            {checked_numeric::narrow<std::uint32_t>(first->as_integer(),
                                                    "/Index first object number"),
             checked_numeric::narrow<std::uint32_t>(count->as_integer(), "/Index object count")});
    }

    return ranges;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
std::uint64_t cross_reference_stream_parser::read_field(std::span<const std::byte> data,
                                                        byte_offset offset, xref_field_width width)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    const auto field =
        checked_subspan(data, offset, byte_count::from_integer(width.value()), "Xref stream field");

    std::uint64_t value = 0;
    for (const auto byte_value : field)
        value =
            (value << 8) | static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(byte_value));

    return value;
}

} // namespace ripper::pdf::core
