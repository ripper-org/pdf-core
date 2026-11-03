#pragma once

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/filter/stream_filter.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{

/// Static utility for managing stream filters and decoding/encoding stream data.
///
/// This type serves as a central repository of filter implementations and provides
/// methods to decode and encode stream bytes using the /Filter entry from a stream
/// dictionary.
///
/// Default filters (e.g. FlateDecode) are registered lazily on first access.
/// Custom filters can be registered globally via `register_filter()`.
///
/// ## Usage
///
/// ```cpp
/// filter_manager::register_filter("MyFilter", std::make_unique<my_filter>());
/// auto decoded = filter_manager::decode(dict, raw_bytes);
/// ```
class filter_manager
{
public:
    filter_manager() = delete;

    /// Register a filter implementation globally.
    static void register_filter(std::string name, std::unique_ptr<stream_filter> impl);

    /// Returns the filter for the given name, or nullptr if not registered.
    /// Lazy-initializes default filters on first call.
    [[nodiscard]] static const stream_filter* get(std::string_view name);

    /// Returns true if a filter with the given name is registered.
    [[nodiscard]] static bool has(std::string_view name);

    /// Remove a registered filter by name. No-op if not found.
    static void forget(std::string_view name);

    /// Decode stream bytes using /Filter from the dictionary.
    ///
    /// If no /Filter is present, returns input as-is.
    /// If /Filter is a single name, applies that filter.
    /// If /Filter is an array of names, applies each filter in order.
    ///
    /// Decoding output (including intermediate filter results) is bounded by
    /// `limits.max_decoded_stream_bytes`.
    ///
    /// @throws parse_exception if /Filter is malformed, a filter is unknown, or
    ///         the decoded output exceeds the configured limit.
    [[nodiscard]] static std::vector<std::byte> decode(const dictionary_object& dict,
                                                       std::span<const std::byte> raw,
                                                       const resource_limits& limits = {});

    /// Encode stream bytes using /Filter from the dictionary.
    ///
    /// If no /Filter is present, returns input as-is.
    /// If /Filter is a single name, applies that filter.
    /// If /Filter is an array of names, applies each filter in reverse order.
    ///
    /// @throws parse_exception if /Filter is malformed or a filter is unknown.
    [[nodiscard]] static std::vector<std::byte> encode(const dictionary_object& dict,
                                                       std::span<const std::byte> decoded);

private:
    static std::unordered_map<std::string, std::unique_ptr<stream_filter>>& filters();
    static void ensure_defaults();
};

} // namespace ripper::pdf::core
