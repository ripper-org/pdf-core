#pragma once

#include "ripper/pdf/core/document/catalog/pages/page/content/content_item.hpp"
#include "ripper/pdf/core/parser/lexer/content_stream_lexer.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
class stream_object;

/// A lazy, read-only view over the decoded bytes of one or more content
/// streams (ISO 32000-1 §7.8.2).
///
/// This is not the content stream itself — the PDF object is `content_stream`.
/// This type is the instruction-level reader over that object's bytes: nothing
/// is materialized up front, so memory stays proportional to one instruction
/// instead of the whole stream.
///
/// The view never modifies the bytes it reads. Constructing one from a
/// `stream_object` decodes the stream into memory owned by the view (or borrows
/// the object's bytes when there is no filter); the on-disk bytes are left
/// untouched, so a save continues to write the original encoded payload.
///
/// `tokens()` yields lexer tokens and `operations()` groups operand runs into
/// `content_operation`/`inline_image` items one at a time. The sequence is
/// deliberately flat: `q`/`Q`, `BT`/`ET`, `BMC`/`BDC`/`EMC`, and `BX`/`EX` are
/// stack operations, not nesting. Resolving them into a tree is a job for a
/// read-side analyzer, never for this view.
///
/// The bytes (or the `stream_object`s) must outlive the view and any iterator
/// obtained from it.
class instruction_stream
{
public:
    /// Forward iterator over raw lexer tokens.
    class token_iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;
        using value_type = content_stream_token;
        using difference_type = std::ptrdiff_t;

        /// Construct the past-the-end iterator.
        token_iterator() noexcept = default;

        /// Tokenize `data`, priming the iterator on the first token.
        ///
        /// @throws parse_exception if the bytes do not follow the content-stream
        ///         grammar, for example an unterminated inline image.
        token_iterator(std::span<const std::byte> data, std::size_t max_depth);

        [[nodiscard]] const content_stream_token& operator*() const noexcept
        {
            return current_;
        }
        [[nodiscard]] const content_stream_token* operator->() const noexcept
        {
            return &current_;
        }

        token_iterator& operator++();
        void operator++(int)
        {
            ++*this;
        }

        friend bool operator==(const token_iterator& lhs, const token_iterator& rhs) noexcept
        {
            return lhs.at_end_ == rhs.at_end_ && (lhs.at_end_ || lhs.lexer_ == rhs.lexer_);
        }

    private:
        std::shared_ptr<content_stream_lexer> lexer_;
        content_stream_token current_{};
        bool at_end_ = true;
    };

    /// Forward iterator over grouped instructions and inline images.
    class operation_iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::input_iterator_tag;
        using value_type = content_item;
        using difference_type = std::ptrdiff_t;

        /// Construct the past-the-end iterator.
        operation_iterator() noexcept = default;

        /// Group operands into instructions, priming the iterator on the first item.
        ///
        /// @throws parse_exception if the bytes do not form a well-shaped content
        ///         stream (dangling operands, unterminated inline image, …).
        operation_iterator(std::span<const std::byte> data, std::size_t max_depth);

        [[nodiscard]] const content_item& operator*() const noexcept
        {
            return current_;
        }
        [[nodiscard]] const content_item* operator->() const noexcept
        {
            return &current_;
        }

        operation_iterator& operator++();
        void operator++(int)
        {
            ++*this;
        }

        friend bool operator==(const operation_iterator& lhs,
                               const operation_iterator& rhs) noexcept
        {
            return lhs.at_end_ == rhs.at_end_ && (lhs.at_end_ || lhs.lexer_ == rhs.lexer_);
        }

    private:
        void advance();

        std::shared_ptr<content_stream_lexer> lexer_;
        std::vector<object> pending_operands_;
        content_item current_{};
        bool at_end_ = true;
    };

    /// Range of raw lexer tokens.
    class token_range
    {
    public:
        token_range() noexcept = default;
        token_range(std::span<const std::byte> data, std::size_t max_depth) noexcept
            : data_{data}, max_depth_{max_depth}
        {
        }

        [[nodiscard]] token_iterator begin() const
        {
            return token_iterator{data_, max_depth_};
        }
        [[nodiscard]] token_iterator end() const noexcept
        {
            return {};
        }

    private:
        std::span<const std::byte> data_{};
        std::size_t max_depth_ = 0;
    };

    /// Range of grouped instructions and inline images.
    class operation_range
    {
    public:
        operation_range() noexcept = default;
        operation_range(std::span<const std::byte> data, std::size_t max_depth) noexcept
            : data_{data}, max_depth_{max_depth}
        {
        }

        [[nodiscard]] operation_iterator begin() const
        {
            return operation_iterator{data_, max_depth_};
        }

        [[nodiscard]] operation_iterator end() const noexcept
        {
            return {};
        }

    private:
        std::span<const std::byte> data_{};
        std::size_t max_depth_ = 0;
    };

    /// Read content-stream bytes. The bytes must outlive the view.
    explicit instruction_stream(std::span<const std::byte> data,
                                const resource_limits& limits = {});

    /// Read the decoded bytes of `object`.
    ///
    /// Decoding is not cached: filtered streams own their decoded bytes for the
    /// lifetime of this view; unfiltered streams borrow the object's stored
    /// bytes. The stream's raw bytes are never modified.
    ///
    /// @throws parse_exception if `/Filter` is unknown or the decoded output
    ///         exceeds `limits.max_decoded_stream_bytes`.
    explicit instruction_stream(const stream_object& object, const resource_limits& limits = {});

    /// Read several content streams as one (for `/Contents` arrays).
    ///
    /// Parts are concatenated with a line break between them, which keeps
    /// tokens separated while preserving each part's decoded bytes.
    explicit instruction_stream(std::span<stream_object* const> objects,
                                const resource_limits& limits = {});

    instruction_stream(const instruction_stream& other);
    instruction_stream& operator=(const instruction_stream& other);
    instruction_stream(instruction_stream&& other) noexcept;
    instruction_stream& operator=(instruction_stream&& other) noexcept;
    ~instruction_stream() = default;

    /// Iterate over raw lexer tokens.
    [[nodiscard]] token_range tokens() const noexcept;

    /// Iterate over grouped instructions and inline images.
    [[nodiscard]] operation_range operations() const noexcept;

    /// Returns the bytes this view reads (decoded, or concatenated when built
    /// from several streams).
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        return data_;
    }

    /// Returns `true` if the view has no bytes.
    [[nodiscard]] bool empty() const noexcept
    {
        return data_.empty();
    }

private:
    void rebind() noexcept;

    std::vector<std::byte> storage_;
    std::span<const std::byte> data_{};
    std::size_t max_depth_ = resource_limits{}.max_parse_depth;
    bool owns_ = false;
};
} // namespace ripper::pdf::core
