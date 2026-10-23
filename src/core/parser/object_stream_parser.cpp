#include "ripper/pdf/core/parser/object_stream_parser.hpp"

#include "object_stream_layout.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/helpers/object_identity.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/lexer/pdf_lexer.hpp"
#include "ripper/pdf/core/parser/value_parsing.hpp"

#include <string_view>

namespace ripper::pdf::core
{

std::vector<indirect_object> object_stream_parser::parse(document& doc,
                                                         const stream_object& stream_obj)
{
    const auto& dict = stream_obj.dictionary();

    const auto* type = dict.get_name("Type");
    if (type == nullptr || type->value != "ObjStm")
        throw parse_exception{"Object stream must have /Type /ObjStm"};

    const auto content = stream_obj.raw();
    const auto layout = object_stream_layout::parse(content, dict, doc.resource_limits());

    std::vector<indirect_object> result;
    result.reserve(layout.entries().size());

    for (const auto& entry : layout.entries())
    {
        // The layout guarantees a non-empty, in-bounds range.
        const auto body = entry.body();

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        std::string_view body_sv{reinterpret_cast<const char*>(body.data()), body.size()};

        pdf_lexer lexer{body_sv};
        auto obj_value = parse_value(lexer, doc.resource_limits().max_parse_depth);

        if (obj_value.is_stream())
            throw parse_exception{
                "Object stream shall not contain stream objects (ISO 32000-1 §7.6.3)"};

        const indirect_reference ref{entry.object_number(), 0};
        result.emplace_back(object_identity{&doc, ref}, std::move(obj_value));
    }

    return result;
}

} // namespace ripper::pdf::core
