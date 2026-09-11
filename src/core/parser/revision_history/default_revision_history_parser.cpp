#include "ripper/pdf/core/parser/revision_history/default_revision_history_parser.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/compressed_cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/default_cross_reference_table_parser.hpp"
#include "ripper/pdf/core/parser/trailer/default_trailer_parser.hpp"
#include "ripper/pdf/core/util/byte.hpp"
#include "ripper/pdf/core/util/text.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
/// Extract the object number from the leading `N G obj` header of an xref stream.
///
/// The xref stream's content begins with an indirect-object header of the form
/// `N G obj\n<<...>>`. This function parses the leading integer `N` so the
/// in-memory section carries the same identity as the file. Returns
/// `std::nullopt` if the header is missing or malformed.
[[nodiscard]] std::optional<std::uint32_t>
extract_xref_stream_object_number(std::string_view content)
{
    auto pos = content.find_first_not_of(" \t\r\n");
    if (pos == std::string_view::npos)
        return std::nullopt;

    const auto num_start = pos;
    for (; pos < content.size(); ++pos)
        if (!std::isdigit(static_cast<unsigned char>(content[pos])))
            break;

    if (pos == num_start)
        return std::nullopt;

    std::uint32_t object_number = 0;
    const auto sv = content.substr(num_start, pos - num_start);

    /// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), object_number);
    /// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    if (ec != std::errc{})
        return std::nullopt;

    return object_number;
}
} // namespace

default_revision_history_parser::default_revision_history_parser(document& document)
    : default_revision_history_parser(document,
                                      std::make_unique<default_cross_reference_table_parser>(),
                                      std::make_unique<default_trailer_parser>())
{
}

default_revision_history_parser::default_revision_history_parser(
    document& document, std::unique_ptr<class cross_reference_table_parser> xref_parser,
    std::unique_ptr<class trailer_parser> trailer_parser)
    : _document{document}, _xref_parser{std::move(xref_parser)},
      _trailer_parser{std::move(trailer_parser)}
{
    if (!_xref_parser)
        _xref_parser = std::make_unique<default_cross_reference_table_parser>();
    if (!_trailer_parser)
        _trailer_parser = std::make_unique<default_trailer_parser>();
}

std::optional<std::size_t>
default_revision_history_parser::extract_prev_offset(const trailer& trailer)
{
    auto prev = trailer.prev();
    if (!prev)
        return std::nullopt;

    return static_cast<std::size_t>(*prev);
}

std::optional<std::size_t>
default_revision_history_parser::find_start_xref_offset(ripper::io::core::reader& reader)
{
    constexpr std::string_view start_xref_keyword = "startxref";
    constexpr std::size_t line_buffer_size = 256;
    constexpr std::size_t search_area_size = 1024;

    const std::uint64_t file_size = reader.size();
    const std::size_t search_pos = file_size > search_area_size ? file_size - search_area_size : 0;

    reader.seek(search_pos);

    std::array<std::byte, line_buffer_size> buffer{};
    std::optional<std::uint64_t> result;

    while (!reader.eof())
    {
        const std::size_t bytes_read = reader.read_line(buffer);
        if (bytes_read == 0)
        {
            break;
        }

        const std::string_view line{byte::as_chars(buffer.data()), bytes_read};

        if (text::starts_with_token(line, start_xref_keyword))
        {
            const std::size_t offset_bytes = reader.read_line(buffer);
            if (offset_bytes == 0)
            {
                break;
            }

            const std::string_view offset_line{byte::as_chars(buffer.data()), offset_bytes};
            const auto offset = text::parse_size_t(offset_line);
            if (offset)
            {
                result = offset;
            }
            else
            {
                break;
            }
        }
    }

    return result;
}

std::unique_ptr<revision_manager> default_revision_history_parser::parse()
{
    auto* reader_ptr = _document.reader();
    if (reader_ptr == nullptr)
        throw io_exception{"No reader backend available"};

    auto& reader = *reader_ptr;

    auto start_xref_result = find_start_xref_offset(reader);
    if (!start_xref_result)
        throw parse_exception{"Missing startxref section"};

    std::deque<revision> revisions;
    std::unordered_set<std::size_t> visited_offsets;
    std::size_t current_offset = *start_xref_result;

    for (;;)
    {
        if (visited_offsets.contains(current_offset))
        {
            break;
        }

        visited_offsets.insert(current_offset);

        reader.seek(current_offset);

        constexpr std::size_t k_peek_size = 64;
        std::array<std::byte, k_peek_size> peek_buffer{};
        const std::size_t peek_bytes = reader.read(peek_buffer);
        if (peek_bytes == 0)
        {
            if (revisions.empty())
                throw parse_exception{
                    "Unable to find complete trailer while parsing document structure"};
            break;
        }

        reader.seek(current_offset);

        std::string_view peek_sv{byte::as_chars(peek_buffer.data()), peek_bytes};
        while (!peek_sv.empty() && (peek_sv.front() == ' ' || peek_sv.front() == '\n' ||
                                    peek_sv.front() == '\r' || peek_sv.front() == '\t'))
            peek_sv.remove_prefix(1);

        if (text::starts_with_token(peek_sv, "xref"))
        {
            std::string collected_content;
            constexpr std::size_t k_buf_size = 4096;
            std::array<std::byte, k_buf_size> buffer{};
            bool has_trailer_end_been_found = false;

            while (!reader.eof() && !has_trailer_end_been_found)
            {
                const std::size_t bytes_read = reader.read(buffer);
                if (bytes_read == 0)
                    break;

                std::string_view chunk{byte::as_chars(buffer.data()), bytes_read};
                collected_content += chunk;

                if (collected_content.find("trailer") != std::string::npos &&
                    collected_content.find(">>") != std::string::npos)
                {
                    has_trailer_end_been_found = true;
                }
            }

            if (!has_trailer_end_been_found)
            {
                if (revisions.empty())
                    throw parse_exception{
                        "Unable to find complete trailer while parsing document structure"};
                break;
            }

            auto xref_section = _xref_parser->parse(collected_content);
            xref_section.set_startxref_offset(static_cast<std::uint64_t>(current_offset));

            auto trailer_result = _trailer_parser->parse(collected_content);

            revisions.emplace_back(std::move(xref_section), std::move(trailer_result));

            const auto* xrefstm = revisions.back().trailer().dictionary().get_number("XRefStm");
            if (xrefstm != nullptr && xrefstm->as_integer() >= 0)
            {
                auto stm_offset = static_cast<std::size_t>(xrefstm->as_integer());
                if (!visited_offsets.contains(stm_offset))
                {
                    visited_offsets.insert(stm_offset);
                    reader.seek(stm_offset);

                    std::string stm_content;
                    constexpr std::size_t k_stm_buf_size = 4096;
                    std::array<std::byte, k_stm_buf_size> stm_buffer{};
                    bool found_stm_endobj = false;

                    while (!reader.eof() && !found_stm_endobj)
                    {
                        const std::size_t stm_bytes_read = reader.read(stm_buffer);
                        if (stm_bytes_read == 0)
                            break;

                        std::string_view stm_chunk{byte::as_chars(stm_buffer.data()),
                                                   stm_bytes_read};
                        stm_content += stm_chunk;

                        if (stm_content.find("endobj") != std::string::npos)
                            found_stm_endobj = true;
                    }

                    if (found_stm_endobj)
                    {
                        auto [stm_section, stm_trailer] =
                            compressed_cross_reference_table_parser::parse(
                                _document, stm_content, indirect_reference{0, 0});
                        for (auto& entry : stm_section.entries())
                        {
                            revisions.back().section().add_entry(std::move(*entry.second));
                        }
                    }
                }
            }

            auto prev_offset_result = extract_prev_offset(revisions.back().trailer());
            if (!prev_offset_result)
                break;

            current_offset = *prev_offset_result;
        }
        else
        {
            std::string collected_content;
            constexpr std::size_t k_buf_size = 4096;
            std::array<std::byte, k_buf_size> buffer{};
            bool found_endobj = false;

            while (!reader.eof() && !found_endobj)
            {
                const std::size_t bytes_read = reader.read(buffer);
                if (bytes_read == 0)
                    break;

                std::string_view chunk{byte::as_chars(buffer.data()), bytes_read};
                collected_content += chunk;

                if (collected_content.find("endobj") != std::string::npos)
                    found_endobj = true;
            }

            if (!found_endobj)
            {
                if (revisions.empty())
                    throw parse_exception{
                        "Unable to find complete trailer while parsing document structure"};
                break;
            }

            auto [section, trailer_obj] = compressed_cross_reference_table_parser::parse(
                _document, collected_content, indirect_reference{0, 0});

            section.set_startxref_offset(static_cast<std::uint64_t>(current_offset));

            if (const auto obj_num = extract_xref_stream_object_number(collected_content))
                section.set_xref_stream_object_number(obj_num);

            revisions.emplace_back(std::move(section), std::move(trailer_obj));

            auto prev_offset_result = extract_prev_offset(revisions.back().trailer());
            if (!prev_offset_result)
                break;

            current_offset = *prev_offset_result;
        }
    }

    std::reverse(revisions.begin(), revisions.end());

    return std::make_unique<revision_manager>(std::move(revisions));
}
} // namespace ripper::pdf::core
