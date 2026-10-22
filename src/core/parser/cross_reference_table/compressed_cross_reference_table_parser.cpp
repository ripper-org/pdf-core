#include "ripper/pdf/core/parser/cross_reference_table/compressed_cross_reference_table_parser.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/cross_reference_table/cross_reference_stream_parser.hpp"
#include "ripper/pdf/core/parser/object_parser.hpp"
#include "ripper/pdf/core/parser/parser.hpp"
#include "ripper/pdf/core/parser/parser_manager.hpp"
#include "ripper/pdf/core/parser/value_parsing.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace ripper::pdf::core
{
namespace
{

/// Parse a `major.minor` PDF version string into numeric components.
[[nodiscard]] std::optional<std::pair<int, int>> parse_version(std::string_view version) noexcept
{
    const auto dot = version.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= version.size())
        return std::nullopt;

    int major = 0;
    for (std::size_t i = 0; i < dot; ++i)
    {
        const char c = version[i];
        if (c < '0' || c > '9')
            return std::nullopt;
        major = major * 10 + (c - '0');
    }

    int minor = 0;
    for (std::size_t i = dot + 1; i < version.size(); ++i)
    {
        const char c = version[i];
        if (c < '0' || c > '9')
            return std::nullopt;
        minor = minor * 10 + (c - '0');
    }

    return std::pair{major, minor};
}

/// Returns `true` when `candidate` is a strictly newer PDF version than `current`.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] bool is_newer_version(std::string_view candidate, std::string_view current) noexcept
{
    const auto a = parse_version(candidate);
    const auto b = parse_version(current);
    if (!a.has_value() || !b.has_value())
        return false;

    if (a->first != b->first)
        return a->first > b->first;
    return a->second > b->second;
}

} // namespace

std::pair<cross_reference_section, trailer>
compressed_cross_reference_table_parser::parse(document& doc, std::string_view content,
                                               indirect_reference temp_ref)
{
    auto* parser = doc.parser();
    if (parser == nullptr)
        throw logic_exception{"No parser available to parse xref stream"};

    content = extract_object_body(content);

    auto parsed_obj = parser->manager().object_parser().parse(doc, temp_ref, content);

    auto* os = parsed_obj.content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Xref stream indirect object is not a stream"};

    (void)os->content(doc.limits());

    auto section = cross_reference_stream_parser::parse(*os);

    trailer trailer_obj{os->dictionary()};

    // PDF 32000-1 §7.5.8: an xref stream dict may carry a higher /Version than
    // the file header. Bubble that up so consumers observing header() see the
    // effective document version.
    if (const auto* version = os->dictionary().get_string("Version"))
    {
        const std::string declared{version->as_string()};
        if (is_newer_version(declared, doc.header().version()))
            doc.header().set_version(declared);
    }

    return {std::move(section), std::move(trailer_obj)};
}

} // namespace ripper::pdf::core
