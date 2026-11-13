#include "ripper/pdf/core/parser/content/content_stream_parser.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/lexer/content_stream_lexer.hpp"

#include <span>
#include <utility>
#include <vector>

namespace ripper::pdf::core
{
content_stream_instructions content_stream_parser::parse(std::span<const std::byte> data,
                                                         const resource_limits& limits)
{
    content_stream_lexer lexer{data, limits.max_parse_depth};

    content_stream_instructions instructions;
    std::vector<object> operands;

    while (true)
    {
        auto token = lexer.next();

        switch (token.kind)
        {
            case content_stream_token_kind::end_of_stream:
                if (!operands.empty())
                    throw parse_exception{
                        "Content stream ends with operands that no operator consumes"};

                return instructions;

            case content_stream_token_kind::operand:
                operands.push_back(std::move(token.operand));
                break;

            case content_stream_token_kind::operator_name:
                instructions.append(
                    content_operation{std::move(token.operator_name), std::move(operands)});
                operands.clear();
                break;

            case content_stream_token_kind::inline_image:
                if (!operands.empty())
                    throw parse_exception{"Inline image begins while operands are still pending"};

                instructions.append(std::move(token.image));
                break;
        }
    }
}
} // namespace ripper::pdf::core
