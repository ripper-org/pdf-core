#pragma once

#include "ripper/pdf/core/filter/stream_filter.hpp"

namespace ripper::pdf::core
{

class flate_decode_filter final : public stream_filter
{
public:
    [[nodiscard]] std::vector<std::byte>
    decode(std::span<const std::byte> input,
           const dictionary_object* params = nullptr) const override;

    [[nodiscard]] std::vector<std::byte>
    decode_with_limits(std::span<const std::byte> input, const dictionary_object* params,
                       const resource_limits& limits) const override;

    [[nodiscard]] std::vector<std::byte>
    encode(std::span<const std::byte> input,
           const dictionary_object* params = nullptr) const override;
};

} // namespace ripper::pdf::core
