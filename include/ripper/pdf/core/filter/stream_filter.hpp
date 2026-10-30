#pragma once

#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ripper::pdf::core
{

class dictionary_object;

class stream_filter
{
public:
    virtual ~stream_filter() = default;

    [[nodiscard]] virtual std::vector<std::byte>
    decode(std::span<const std::byte> input, const dictionary_object* params = nullptr) const = 0;

    /// Decode `input` while respecting `limits`.
    ///
    /// The default implementation forwards to `decode()` and therefore cannot
    /// enforce limits from within the filter. The filter manager always checks
    /// the cumulative output afterwards; filters that can produce large output
    /// from small input should override this to stop early.
    [[nodiscard]] virtual std::vector<std::byte>
    decode_with_limits(std::span<const std::byte> input, const dictionary_object* params,
                       const resource_limits& limits) const
    {
        (void)limits;
        return decode(input, params);
    }

    [[nodiscard]] virtual std::vector<std::byte>
    encode(std::span<const std::byte> input, const dictionary_object* params = nullptr) const = 0;
};

} // namespace ripper::pdf::core
