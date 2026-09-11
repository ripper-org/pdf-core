#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_stream_parser.hpp"

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_subsection.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

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
    if (size_ptr == nullptr || size_ptr->as_integer() <= 0)
        throw parse_exception{"Xref stream missing required /Size"};

    const auto size = static_cast<std::uint32_t>(size_ptr->as_integer());
    const auto widths = parse_w(stream_obj);
    const auto ranges = parse_index(stream_obj, size);

    const auto entry_size = widths.w0 + widths.w1 + widths.w2;
    if (entry_size == 0)
        throw parse_exception{"Xref stream entry size is zero"};

    cross_reference_section section{std::deque<cross_reference_subsection>{}};
    std::size_t byte_offset = 0;

    for (const auto& range : ranges)
    {
        for (std::uint32_t i = 0; i < range.count; ++i)
        {
            if (byte_offset + entry_size > content.size())
                throw parse_exception{"Xref stream data truncated"};

            const auto entry_data = content.subspan(byte_offset, entry_size);

            const auto type_raw = widths.w0 == 0 ? 1 : read_field(entry_data, 0, widths.w0);
            const auto field1 = read_field(entry_data, widths.w0, widths.w1);
            const auto field2 =
                widths.w2 > 0 ? read_field(entry_data, widths.w0 + widths.w1, widths.w2) : 0;

            const auto obj_num = range.first + i;

            const auto type = static_cast<xref_entry_type>(type_raw);

            switch (type)
            {
                case xref_entry_type::free:
                {
                    section.add_entry(cross_reference_entry::make_free(
                        indirect_reference{obj_num, 0}, static_cast<std::uint32_t>(field1),
                        static_cast<std::uint16_t>(field2)));
                    break;
                }
                case xref_entry_type::uncompressed:
                {
                    section.add_entry(cross_reference_entry{
                        indirect_reference{obj_num, static_cast<std::uint16_t>(field2)}, field1,
                        true});
                    break;
                }
                case xref_entry_type::compressed:
                {
                    section.add_entry(cross_reference_entry{indirect_reference{obj_num, 0},
                                                            static_cast<std::uint32_t>(field1),
                                                            static_cast<std::uint32_t>(field2)});
                    break;
                }
                default:
                    section.add_entry(cross_reference_entry::make_free(
                        indirect_reference{obj_num, 0}, static_cast<std::uint32_t>(field1),
                        static_cast<std::uint16_t>(field2)));
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
    if (w == nullptr || w->size() < 3)
        throw parse_exception{"Xref stream missing or invalid /W array"};

    column_widths result;
    const auto* w0 = w->at(0).as_number();
    const auto* w1 = w->at(1).as_number();
    const auto* w2 = w->at(2).as_number();

    if (w0 == nullptr || w1 == nullptr || w2 == nullptr)
        throw parse_exception{"/W array entries must be integers"};

    result.w0 = static_cast<std::uint32_t>(w0->as_integer());
    result.w1 = static_cast<std::uint32_t>(w1->as_integer());
    result.w2 = static_cast<std::uint32_t>(w2->as_integer());

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
    for (std::size_t i = 0; i < index->size(); i += 2)
    {
        const auto* first = index->at(i).as_number();
        const auto* count = index->at(i + 1).as_number();

        if (first == nullptr || count == nullptr)
            throw parse_exception{"/Index array entries must be integers"};

        ranges.push_back({static_cast<std::uint32_t>(first->as_integer()),
                          static_cast<std::uint32_t>(count->as_integer())});
    }

    return ranges;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
std::uint64_t cross_reference_stream_parser::read_field(std::span<const std::byte> data,
                                                        std::size_t offset, std::uint32_t width)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i)
    {
        value = (value << 8) |
                static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(data[offset + i]));
    }
    return value;
}

} // namespace ripper::pdf::core
