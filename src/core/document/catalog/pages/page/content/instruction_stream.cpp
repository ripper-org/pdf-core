#include "ripper/pdf/core/document/catalog/pages/page/content/instruction_stream.hpp"

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <utility>

namespace ripper::pdf::core
{
instruction_stream::instruction_stream(std::span<const std::byte> data,
                                       const resource_limits& limits)
    : data_{data}, max_depth_{limits.max_parse_depth}
{
}

instruction_stream::instruction_stream(const stream_object& object, const resource_limits& limits)
    : max_depth_{limits.max_parse_depth}
{
    auto decoded = object.decode(limits);

    if (decoded.owns())
    {
        auto owned = std::move(decoded).release();
        storage_ = std::move(*owned);
        owns_ = true;
        rebind();
        return;
    }

    data_ = decoded.bytes();
}

instruction_stream::instruction_stream(std::span<stream_object* const> objects,
                                       const resource_limits& limits)
    : max_depth_{limits.max_parse_depth}
{
    if (objects.size() == 1)
    {
        auto decoded = objects.front()->decode(limits);

        if (decoded.owns())
        {
            auto owned = std::move(decoded).release();
            storage_ = std::move(*owned);
            owns_ = true;
            rebind();
            return;
        }

        data_ = decoded.bytes();
        return;
    }

    for (std::size_t i = 0; i < objects.size(); ++i)
    {
        if (i > 0)
            storage_.push_back(static_cast<std::byte>('\n'));

        auto decoded = objects[i]->decode(limits);
        const auto bytes = decoded.bytes();
        storage_.insert(storage_.end(), bytes.begin(), bytes.end());
    }

    owns_ = true;
    rebind();
}

instruction_stream::instruction_stream(const instruction_stream& other)
    : storage_(other.storage_), data_(other.data_), max_depth_(other.max_depth_), owns_(other.owns_)
{
    rebind();
}

instruction_stream& instruction_stream::operator=(const instruction_stream& other)
{
    if (this == &other)
        return *this;

    storage_ = other.storage_;
    data_ = other.data_;
    max_depth_ = other.max_depth_;
    owns_ = other.owns_;
    rebind();

    return *this;
}

instruction_stream::instruction_stream(instruction_stream&& other) noexcept
    : storage_(std::move(other.storage_)), data_(other.data_), max_depth_(other.max_depth_),
      owns_(other.owns_)
{
    rebind();
    other.data_ = {};
    other.owns_ = false;
}

instruction_stream& instruction_stream::operator=(instruction_stream&& other) noexcept
{
    if (this == &other)
        return *this;

    storage_ = std::move(other.storage_);
    data_ = other.data_;
    max_depth_ = other.max_depth_;
    owns_ = other.owns_;
    rebind();

    other.data_ = {};
    other.owns_ = false;

    return *this;
}

void instruction_stream::rebind() noexcept
{
    if (owns_)
        data_ = storage_;
}

instruction_stream::token_range instruction_stream::tokens() const noexcept
{
    return token_range{data_, max_depth_};
}

instruction_stream::operation_range instruction_stream::operations() const noexcept
{
    return operation_range{data_, max_depth_};
}

instruction_stream::token_iterator::token_iterator(std::span<const std::byte> data,
                                                   std::size_t max_depth)
    : lexer_{std::make_shared<content_stream_lexer>(data, max_depth)}
{
    current_ = lexer_->next();

    if (current_.kind == content_stream_token_kind::end_of_stream)
    {
        lexer_.reset();
        return;
    }

    at_end_ = false;
}

instruction_stream::token_iterator& instruction_stream::token_iterator::operator++()
{
    if (at_end_)
        return *this;

    current_ = lexer_->next();

    if (current_.kind == content_stream_token_kind::end_of_stream)
    {
        lexer_.reset();
        at_end_ = true;
    }

    return *this;
}

instruction_stream::operation_iterator::operation_iterator(std::span<const std::byte> data,
                                                           std::size_t max_depth)
    : lexer_{std::make_shared<content_stream_lexer>(data, max_depth)}
{
    advance();
}

instruction_stream::operation_iterator& instruction_stream::operation_iterator::operator++()
{
    if (at_end_)
        return *this;

    advance();

    return *this;
}

void instruction_stream::operation_iterator::advance()
{
    while (lexer_ != nullptr)
    {
        auto token = lexer_->next();

        switch (token.kind)
        {
            case content_stream_token_kind::end_of_stream:
                if (!pending_operands_.empty())
                    throw parse_exception{
                        "Content stream ends with operands that no operator consumes"};

                at_end_ = true;
                lexer_.reset();
                return;

            case content_stream_token_kind::operand:
                pending_operands_.push_back(std::move(token.operand));
                break;

            case content_stream_token_kind::operator_name:
                current_ =
                    content_operation{std::move(token.operator_name), std::move(pending_operands_)};
                pending_operands_.clear();
                at_end_ = false;
                return;

            case content_stream_token_kind::inline_image:
                if (!pending_operands_.empty())
                    throw parse_exception{"Inline image begins while operands are still pending"};

                current_ = std::move(token.image);
                at_end_ = false;
                return;
        }
    }
}
} // namespace ripper::pdf::core
