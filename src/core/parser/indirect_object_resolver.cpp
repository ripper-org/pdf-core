#include "ripper/pdf/core/parser/indirect_object_resolver.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/objstm.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/lexer/pdf_lexer.hpp"
#include "ripper/pdf/core/parser/value_parsing.hpp"
#include "ripper/pdf/core/util/checked.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace ripper::pdf::core
{
indirect_object_resolver::indirect_object_resolver(document& document) : document_{document} {}

std::string indirect_object_resolver::resolve(indirect_reference ref) const
{
    auto* reader_ptr = document_.reader();
    if (reader_ptr == nullptr)
        throw io_exception{"No reader backend available"};

    auto& r = *reader_ptr;

    if (!r.is_open())
        throw io_exception{
            "The provided reader is not open while trying to resolve an indirect object"};

    auto& xref = document_.cross_reference_table();
    const auto entry = xref.find(ref);
    if (entry == nullptr)
        throw parse_exception{"XRef entry not found for indirect object"};

    if (!entry->in_use())
        throw parse_exception{"XRef entry is not in use"};

    if (entry->is_compressed())
        return resolve_compressed(ref, *entry);

    return resolve_uncompressed(ref, *entry, r);
}

std::string indirect_object_resolver::resolve_uncompressed(indirect_reference ref,
                                                           const cross_reference_entry& entry,
                                                           ripper::io::core::reader& r) const
{
    const auto target_offset = entry.offset();
    if (target_offset == 0)
        throw parse_exception{"XRef entry is missing file offset"};

    const std::uint64_t file_size = r.size();
    if (file_size == 0)
        throw io_exception{"Read size zero while trying to resolve an indirect object"};

    if (target_offset >= file_size)
        throw parse_exception{"Object offset is out of bounds"};

    auto& xref = document_.cross_reference_table();

    // Determine the byte range occupied by this object on disk
    std::optional<std::uint64_t> next_offset;

    for (auto& rev : document_.revisions().all())
    {
        auto& section = rev.section();
        for (const auto& entry_pair : section.entries())
        {
            const auto& off = entry_pair.second->offset();
            if (entry_pair.second->in_use() && off > 0)
            {
                const auto off_val = off;
                if (off_val > target_offset)
                {
                    if (!next_offset.has_value() || off_val < *next_offset)
                        next_offset = off_val;
                }
            }
        }

        const auto sxref_off = section.startxref_offset();
        if (sxref_off.has_value())
        {
            const auto sx_val = *sxref_off;
            if (sx_val > target_offset)
            {
                if (!next_offset.has_value() || sx_val < *next_offset)
                    next_offset = sx_val;
            }
        }
    }

    const std::uint64_t read_end = next_offset.value_or(file_size);
    const std::uint64_t read_size_u64 = read_end - target_offset;

    if (read_size_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        throw parse_exception{"Object is too large to handle"};

    const std::size_t read_size = static_cast<std::size_t>(read_size_u64);
    const std::size_t offset = static_cast<std::size_t>(target_offset);

    std::vector<std::byte> bytes(read_size);
    const std::size_t bytes_read =
        r.read_at(std::span<std::byte>{bytes.data(), bytes.size()}, offset);
    if (bytes_read == 0)
        throw io_exception{
            "Received zero bytes from reader while attempting to read indirect object content"};

    std::string source(bytes_read, '\0');
    for (std::size_t i = 0; i < bytes_read; ++i)
        source[i] = static_cast<char>(bytes[i]);

    pdf_lexer lexer{source};

    auto n_token = lexer.next();
    if (n_token.type != lexer_token_type::integer)
        throw parse_exception{"Expected object number in indirect object header"};

    const auto parsed_obj = text::parse_u32(n_token.lexeme);
    if (!parsed_obj || *parsed_obj != ref.object_number())
        throw parse_exception{"Object number mismatch in indirect object header"};

    auto g_token = lexer.next();
    if (g_token.type != lexer_token_type::integer)
        throw parse_exception{"Expected generation number in indirect object header"};

    const auto parsed_gen = text::parse_u16(g_token.lexeme);

    // Generation number paranoia: a malformed generation field is an error
    // regardless of the expected value.
    if (!parsed_gen.has_value())
        throw parse_exception{"Malformed generation number in indirect object header"};

    if (*parsed_gen != ref.generation())
        throw parse_exception{"Generation number mismatch in indirect object header"};

    auto obj_token = lexer.next();
    if (obj_token.type != lexer_token_type::keyword || obj_token.lexeme != "obj")
        throw parse_exception{"Missing obj marker for indirect object"};

    /// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto value_start =
        static_cast<std::size_t>(obj_token.lexeme.data() + obj_token.lexeme.size() - source.data());
    /// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    auto endobj_pos = text::find_endobj(source);
    if (endobj_pos == std::string::npos)
        throw parse_exception{"Missing endobj marker for indirect object " +
                              std::to_string(ref.object_number()) + " (offset " +
                              std::to_string(target_offset) + ", bound " +
                              std::to_string(read_end) + ")"};

    // When the object is a /Type /Stream with an authoritative /Length, use it to
    // locate `endobj` precisely (PDF 32000-1 §7.3.8) instead of relying on the
    // keyword search, which can misfire when the raw stream bytes contain
    // `endobj`-like sequences. Falls back to the keyword boundary on any error.
    try
    {
        auto value = parse_value(lexer, document_.limits().max_parse_depth);
        if (value.is_dictionary())
        {
            auto peek_result = lexer.peek();
            if (peek_result.type == lexer_token_type::keyword && peek_result.lexeme == "stream")
            {
                auto stream_tok = lexer.next();

                /// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto data_start = static_cast<std::size_t>(
                    stream_tok.lexeme.data() + stream_tok.lexeme.size() - source.data());
                /// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

                while (data_start < source.size() &&
                       (source[data_start] == '\r' || source[data_start] == '\n' ||
                        source[data_start] == ' '))
                {
                    ++data_start;
                }

                const auto* length_ptr = value.as_dictionary()->get_number("Length");
                if (length_ptr != nullptr && length_ptr->is_integer() &&
                    length_ptr->as_integer() >= 0)
                {
                    const auto length = checked::to_unsigned<std::size_t>(length_ptr->as_integer(),
                                                                          "Stream /Length");

                    // Only refine when /Length describes a range inside the buffer.
                    if (data_start <= source.size() && length <= source.size() - data_start)
                    {
                        std::size_t pos = data_start + length;
                        while (pos < source.size() &&
                               (source[pos] == '\r' || source[pos] == '\n' || source[pos] == ' '))
                        {
                            ++pos;
                        }

                        if (source.size() - pos >= 9 && source.substr(pos, 9) == "endstream")
                        {
                            pos += 9;
                            while (
                                pos < source.size() &&
                                (source[pos] == '\r' || source[pos] == '\n' || source[pos] == ' '))
                            {
                                ++pos;
                            }

                            if (source.size() - pos >= 6 && source.substr(pos, 6) == "endobj")
                                endobj_pos = pos;
                        }
                    }
                }
            }
        }
    }
    // Best-effort refinement: a malformed /Length is intentionally ignored in
    // favor of the keyword-derived boundary computed above.
    // NOLINTNEXTLINE(bugprone-empty-catch)
    catch (const parse_exception&)
    {
    }

    if (endobj_pos < value_start)
        throw parse_exception{"Malformed indirect object: endobj precedes object content"};

    return source.substr(value_start, endobj_pos - value_start);
}

std::string indirect_object_resolver::resolve_compressed(indirect_reference ref,
                                                         const cross_reference_entry& entry) const
{
    const auto objstm_num = entry.objstm_number();
    const auto objstm_index = entry.objstm_index();

    // Resolve the containing object stream (type-1, handled normally)
    auto& doc = const_cast<document&>(document_);
    auto* objstm_obj = doc.resolve_object(indirect_reference{objstm_num, 0});
    if (objstm_obj == nullptr)
        throw parse_exception{"Object stream " + std::to_string(objstm_num) + " not found"};

    auto* os = objstm_obj->content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object stream " + std::to_string(objstm_num) + " is not a stream"};

    // Decode if needed
    (void)os->content(document_.limits());

    // Find the byte range for the requested object
    objstm view{*objstm_obj};
    auto range = view.object_offset(objstm_index);
    if (!range.has_value())
        throw parse_exception{"Object " + std::to_string(ref.object_number()) +
                              " not found in object stream " + std::to_string(objstm_num)};

    // Copy the bytes
    auto raw = os->raw();
    checked::require_range(range->offset, range->length, raw.size(), "Object stream byte range");

    std::string result(range->length, '\0');
    for (std::size_t i = 0; i < range->length; ++i)
        result[i] = static_cast<char>(raw[range->offset + i]);

    return result;
}

} // namespace ripper::pdf::core
