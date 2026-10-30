#include "ripper/pdf/core/filter/lzw_decode_filter.hpp"

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ripper::pdf::core
{
namespace
{

[[nodiscard]] bool get_early_change(const dictionary_object* params)
{
    if (params == nullptr)
        return true;

    const auto* ec = params->get("EarlyChange");
    if (ec == nullptr)
        return true;

    const auto* num = ec->as_number();
    if (num == nullptr)
        return true;

    return static_cast<int>(num->as_integer()) != 0;
}

} // namespace

std::vector<std::byte> lzw_decode_filter::decode(std::span<const std::byte> input,
                                                 const dictionary_object* params) const
{
    return decode_with_limits(input, params, {});
}

std::vector<std::byte> lzw_decode_filter::decode_with_limits(std::span<const std::byte> input,
                                                             const dictionary_object* params,
                                                             const resource_limits& limits) const
{
    if (input.empty())
        return {};

    const bool early_change = get_early_change(params);

    constexpr std::uint16_t clear_code = 256;
    constexpr std::uint16_t eod_code = 257;
    constexpr std::uint16_t first_code = 258;
    constexpr std::uint16_t max_code = 4095;

    struct table_entry
    {
        std::uint16_t prefix;
        std::byte suffix;
    };

    std::array<table_entry, 4096> table{};
    std::uint16_t next_code = first_code;
    int code_size = 9;
    int code_limit = 0;
    auto update_limit = [&]() { code_limit = (1 << code_size) - (early_change ? 1 : 0); };
    update_limit();

    std::vector<std::byte> output;
    output.reserve(input.size() * 2);

    const auto ensure_output_limit = [&]()
    {
        if (output.size() > limits.max_decoded_stream_bytes)
            throw parse_exception{"Decoded stream exceeds the configured size limit"};
    };

    std::size_t bit_pos = 0;

    auto read_code = [&]() -> std::uint16_t
    {
        std::uint32_t code = 0;
        for (int i = 0; i < code_size; ++i)
        {
            const std::size_t byte_idx = bit_pos / 8;
            const int bit_idx = 7 - static_cast<int>(bit_pos % 8);

            if (byte_idx >= input.size())
                throw parse_exception{"LZWDecode: unexpected end of data"};

            const auto byte_val = static_cast<unsigned char>(input[byte_idx]);
            code = (code << 1) | ((byte_val >> bit_idx) & 1);
            ++bit_pos;
        }
        return static_cast<std::uint16_t>(code);
    };

    auto maybe_increase_code_size = [&]()
    {
        if (code_size < 12 && next_code >= static_cast<std::uint16_t>(code_limit))
        {
            ++code_size;
            update_limit();
        }
    };

    auto add_entry = [&](std::uint16_t prefix, std::byte suffix)
    {
        if (next_code <= max_code)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            table[next_code] = {prefix, suffix};
            ++next_code;
        }
    };

    auto output_code = [&](std::uint16_t code)
    {
        std::array<std::byte, 4096> stack{};
        std::size_t stack_top = 0;

        while (code > 255)
        {
            if (code >= next_code)
                throw parse_exception{"LZWDecode: invalid code in stream"};

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            stack[stack_top++] = table[code].suffix;
            code = table[code].prefix;
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        stack[stack_top++] = static_cast<std::byte>(static_cast<unsigned char>(code));

        while (stack_top > 0)
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            output.push_back(stack[--stack_top]);
    };

    std::uint16_t old_code = read_code();
    if (old_code == eod_code)
        return output;

    if (old_code != clear_code)
        output_code(old_code);

    ensure_output_limit();

    for (;;)
    {
        ensure_output_limit();
        maybe_increase_code_size();

        const std::uint16_t code = read_code();

        if (code == eod_code)
            break;

        if (code == clear_code)
        {
            next_code = first_code;
            code_size = 9;
            update_limit();

            old_code = read_code();
            if (old_code == eod_code)
                break;

            output_code(old_code);
            continue;
        }

        if (code < next_code)
        {
            const std::size_t prev_size = output.size();
            output_code(code);
            if (old_code != clear_code)
                add_entry(old_code, output[prev_size]);
        }
        else if (code == next_code)
        {
            std::array<std::byte, 4096> stack{};
            std::size_t stack_top = 0;
            std::uint16_t chain = old_code;

            while (chain > 255)
            {
                if (chain >= next_code)
                    throw parse_exception{"LZWDecode: invalid code in stream"};

                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
                stack[stack_top++] = table[chain].suffix;
                chain = table[chain].prefix;
                // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            stack[stack_top++] = static_cast<std::byte>(static_cast<unsigned char>(chain));

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            const std::byte first_byte = stack[stack_top - 1];

            while (stack_top > 0)
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                output.push_back(stack[--stack_top]);

            output.push_back(first_byte);
            add_entry(old_code, first_byte);
        }
        else
        {
            throw parse_exception{"LZWDecode: code out of range"};
        }

        old_code = code;
    }

    return output;
}

std::vector<std::byte> lzw_decode_filter::encode(std::span<const std::byte> input,
                                                 const dictionary_object* params) const
{
    if (input.empty())
        return {};

    const bool early_change = get_early_change(params);

    constexpr std::uint16_t clear_code = 256;
    constexpr std::uint16_t eod_code = 257;
    constexpr std::uint16_t first_code = 258;
    constexpr std::uint16_t max_code = 4095;

    std::unordered_map<std::uint32_t, std::uint16_t> dict;

    std::vector<std::byte> output;
    output.reserve(input.size() + 2);
    std::size_t bit_pos = 0;

    int code_size = 9;
    std::uint16_t next_code = first_code;

    auto emit_code = [&](std::uint16_t code)
    {
        for (int i = code_size - 1; i >= 0; --i)
        {
            const std::size_t byte_idx = bit_pos / 8;
            if (byte_idx >= output.size())
                output.push_back(std::byte{0});

            const int bit_idx = 7 - static_cast<int>(bit_pos % 8);
            if ((code >> i) & 1)
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                output[byte_idx] |= std::byte(1 << bit_idx);

            ++bit_pos;
        }
    };

    auto maybe_increase_code_size = [&]()
    {
        if (code_size >= 12)
            return;

        const std::uint32_t x =
            static_cast<std::uint32_t>(next_code) + (early_change ? 1U : 0U) - 1U;
        if (x != 0 && (x & (x - 1U)) == 0U)
            ++code_size;
    };

    auto maybe_increase_code_size_for_final = [&]()
    {
        if (code_size >= 12)
            return;

        // The decoder appends a table entry for the last data code as well, so
        // its code-size increase that applies to the following EOD code is based
        // on one entry beyond the encoder's current table.
        const std::uint32_t x = static_cast<std::uint32_t>(next_code) + (early_change ? 1U : 0U);
        if (x != 0 && (x & (x - 1U)) == 0U)
            ++code_size;
    };

    auto reset_table = [&]()
    {
        dict.clear();
        next_code = first_code;
        code_size = 9;
    };

    emit_code(clear_code);

    std::uint16_t prefix = static_cast<unsigned char>(input[0]);

    for (std::size_t i = 1; i < input.size(); ++i)
    {
        const auto byte_val = static_cast<unsigned char>(input[i]);
        const std::uint32_t key = (static_cast<std::uint32_t>(prefix) << 8) | byte_val;

        auto it = dict.find(key);
        if (it != dict.end())
        {
            prefix = it->second;
            continue;
        }

        emit_code(prefix);

        if (next_code <= max_code)
        {
            dict[key] = next_code;
            ++next_code;
            maybe_increase_code_size();
        }
        else
        {
            emit_code(clear_code);
            reset_table();
        }

        prefix = byte_val;
    }

    emit_code(prefix);
    maybe_increase_code_size_for_final();
    emit_code(eod_code);

    return output;
}

} // namespace ripper::pdf::core
