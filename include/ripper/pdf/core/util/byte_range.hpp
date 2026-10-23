#pragma once

#include "ripper/pdf/core/util/byte_count.hpp"
#include "ripper/pdf/core/util/byte_offset.hpp"
#include "ripper/pdf/core/util/checked_numeric.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace ripper::pdf::core
{
/// Return the subspan described by `offset` and `length`.
///
/// This combines offset/count values with the actual buffer size, which is the
/// only context in which a range can be validated. The returned span borrows
/// from `buffer`; it does not extend the buffer's lifetime.
///
/// @throws parse_exception if the range is not fully contained in `buffer`.
[[nodiscard]] inline std::span<const std::byte>
checked_subspan(std::span<const std::byte> buffer, byte_offset offset, byte_count length,
                std::string_view what = "byte range")
{
    checked_numeric::require_range(offset.value(), length.value(), buffer.size(), what);

    return buffer.subspan(offset.value(), length.value());
}
} // namespace ripper::pdf::core
