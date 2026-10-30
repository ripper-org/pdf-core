#include "ripper/pdf/core/filter/filter_manager.hpp"

#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/filter/ascii_85_decode_filter.hpp"
#include "ripper/pdf/core/filter/ascii_hex_decode_filter.hpp"
#include "ripper/pdf/core/filter/ccitt_fax_decode_filter.hpp"
#include "ripper/pdf/core/filter/crypt_filter.hpp"
#include "ripper/pdf/core/filter/dct_decode_filter.hpp"
#include "ripper/pdf/core/filter/flate_decode_filter.hpp"
#include "ripper/pdf/core/filter/jbig2_decode_filter.hpp"
#include "ripper/pdf/core/filter/jpx_decode_filter.hpp"
#include "ripper/pdf/core/filter/lzw_decode_filter.hpp"
#include "ripper/pdf/core/filter/predictor.hpp"
#include "ripper/pdf/core/filter/run_length_decode_filter.hpp"

#include <unordered_map>
#include <utility>

namespace ripper::pdf::core
{
namespace
{

struct filter_step
{
    const stream_filter* filter;
    const dictionary_object* decode_params;
};

struct filter_chain
{
    std::vector<filter_step> steps;
};

filter_chain build_chain(const dictionary_object& dict)
{
    const auto* filter_entry = dict.get("Filter");
    if (filter_entry == nullptr)
    {
        return {};
    }

    filter_chain chain;

    const auto* filter_name = filter_entry->as_name();
    if (filter_name != nullptr)
    {
        const auto* filter = filter_manager::get(filter_name->value);
        if (filter == nullptr)
        {
            throw parse_exception{std::string{"Unknown filter: "} + filter_name->value};
        }
        const auto* decode_parms = dict.get("DecodeParms");
        chain.steps.push_back(
            {filter, decode_parms != nullptr ? decode_parms->as_dictionary() : nullptr});
        return chain;
    }

    const auto* filter_array = filter_entry->as_array();
    if (filter_array == nullptr)
    {
        throw parse_exception{"/Filter must be a name or an array of names"};
    }

    const std::size_t filter_count = filter_array->size();

    const auto* decode_parms_entry = dict.get("DecodeParms");
    const dictionary_object* shared_decode_parms = nullptr;
    const array_object* decode_parms_array = nullptr;

    if (decode_parms_entry != nullptr)
    {
        shared_decode_parms = decode_parms_entry->as_dictionary();
        if (shared_decode_parms == nullptr)
        {
            decode_parms_array = decode_parms_entry->as_array();
        }
    }

    chain.steps.reserve(filter_count);
    for (std::size_t i = 0; i < filter_count; ++i)
    {
        const auto& entry = (*filter_array)[i];
        const auto* name = entry.as_name();
        if (name == nullptr)
        {
            throw parse_exception{"Filter array entry is not a name"};
        }
        const auto* filter = filter_manager::get(name->value);
        if (filter == nullptr)
        {
            throw parse_exception{std::string{"Unknown filter: "} + name->value};
        }

        const dictionary_object* step_params = nullptr;
        if (shared_decode_parms != nullptr)
        {
            step_params = shared_decode_parms;
        }
        else if (decode_parms_array != nullptr && i < decode_parms_array->size())
        {
            step_params = (*decode_parms_array)[i].as_dictionary();
        }

        chain.steps.push_back({filter, step_params});
    }

    return chain;
}

} // namespace

std::unordered_map<std::string, std::unique_ptr<stream_filter>>& filter_manager::filters()
{
    static std::unordered_map<std::string, std::unique_ptr<stream_filter>> instance;
    return instance;
}

void filter_manager::ensure_defaults()
{
    static bool initialized = []
    {
        register_filter("FlateDecode", std::make_unique<flate_decode_filter>());
        register_filter("LZWDecode", std::make_unique<lzw_decode_filter>());
        register_filter("ASCII85Decode", std::make_unique<ascii_85_decode_filter>());
        register_filter("ASCIIHexDecode", std::make_unique<ascii_hex_decode_filter>());
        register_filter("RunLengthDecode", std::make_unique<run_length_decode_filter>());
        register_filter("CCITTFaxDecode", std::make_unique<ccitt_fax_decode_filter>());
        register_filter("DCTDecode", std::make_unique<dct_decode_filter>());
        register_filter("JPXDecode", std::make_unique<jpx_decode_filter>());
        register_filter("JBIG2Decode", std::make_unique<jbig2_decode_filter>());
        register_filter("Crypt", std::make_unique<crypt_filter>());
        return true;
    }();
    (void)initialized;
}

void filter_manager::register_filter(std::string name, std::unique_ptr<stream_filter> impl)
{
    filters()[std::move(name)] = std::move(impl);
}

const stream_filter* filter_manager::get(std::string_view name)
{
    ensure_defaults();
    const auto it = filters().find(std::string{name});
    return it != filters().end() ? it->second.get() : nullptr;
}

bool filter_manager::has(std::string_view name)
{
    return get(name) != nullptr;
}

void filter_manager::forget(std::string_view name)
{
    filters().erase(std::string{name});
}

std::vector<std::byte> filter_manager::decode(const dictionary_object& dict,
                                              std::span<const std::byte> raw,
                                              const resource_limits& limits)
{
    const auto enforce_limit = [&](std::vector<std::byte> result) -> std::vector<std::byte>
    {
        if (result.size() > limits.max_decoded_stream_bytes)
            throw parse_exception{"Decoded stream exceeds the configured size limit"};

        return result;
    };

    const auto chain = build_chain(dict);
    if (chain.steps.empty())
    {
        if (raw.size() > limits.max_decoded_stream_bytes)
            throw parse_exception{"Decoded stream exceeds the configured size limit"};

        auto result = std::vector<std::byte>{raw.begin(), raw.end()};
        const auto* decode_params = dict.get("DecodeParms");
        const auto* params = decode_params != nullptr ? decode_params->as_dictionary() : nullptr;

        if (params != nullptr && has_predictor(params))
            result = apply_predictor(result, *params, limits);

        return enforce_limit(std::move(result));
    }

    auto result =
        chain.steps[0].filter->decode_with_limits(raw, chain.steps[0].decode_params, limits);
    result = enforce_limit(std::move(result));

    if (has_predictor(chain.steps[0].decode_params))
    {
        result = apply_predictor(result, *chain.steps[0].decode_params, limits);
        result = enforce_limit(std::move(result));
    }

    for (std::size_t i = 1; i < chain.steps.size(); ++i)
    {
        result =
            chain.steps[i].filter->decode_with_limits(result, chain.steps[i].decode_params, limits);
        result = enforce_limit(std::move(result));

        if (has_predictor(chain.steps[i].decode_params))
        {
            result = apply_predictor(result, *chain.steps[i].decode_params, limits);
            result = enforce_limit(std::move(result));
        }
    }

    return result;
}

std::vector<std::byte> filter_manager::encode(const dictionary_object& dict,
                                              std::span<const std::byte> decoded)
{
    const auto chain = build_chain(dict);
    if (chain.steps.empty())
    {
        return {decoded.begin(), decoded.end()};
    }

    auto result = chain.steps.back().filter->encode(decoded, chain.steps.back().decode_params);
    for (auto it = chain.steps.rbegin() + 1; it != chain.steps.rend(); ++it)
    {
        result = it->filter->encode(result, it->decode_params);
    }
    return result;
}

} // namespace ripper::pdf::core
