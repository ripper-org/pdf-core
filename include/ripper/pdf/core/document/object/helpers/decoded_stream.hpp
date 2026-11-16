#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace ripper::pdf::core
{
/// Decoded bytes of a PDF stream.
///
/// Holds either a borrowed view of the stream's stored bytes (when no filter
/// applies) or an owned, freshly decoded buffer. A borrowed view is valid only
/// while the `stream_object` it came from is alive.
///
/// There is no cache: every `stream_object::decode()` call produces a new
/// value. Callers that need the bytes beyond one expression keep the value
/// (owned buffers move into it) or keep the source stream alive (borrowed
/// views).
class decoded_stream
{
public:
    /// Wrap bytes owned elsewhere. The caller guarantees their lifetime.
    explicit decoded_stream(std::span<const std::byte> data) noexcept : data_{data} {}

    /// Take ownership of `data`.
    explicit decoded_stream(std::vector<std::byte> data) noexcept : data_{std::move(data)} {}

    decoded_stream(const decoded_stream&) = delete;
    decoded_stream& operator=(const decoded_stream&) = delete;
    decoded_stream(decoded_stream&&) noexcept = default;
    decoded_stream& operator=(decoded_stream&&) noexcept = default;
    ~decoded_stream() = default;

    /// Returns the decoded bytes.
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        if (const auto* borrowed = std::get_if<std::span<const std::byte>>(&data_))
            return *borrowed;

        const auto& owned = std::get<std::vector<std::byte>>(data_);
        return {owned.data(), owned.size()};
    }

    /// Returns true when this value owns its buffer.
    [[nodiscard]] bool owns() const noexcept
    {
        return std::holds_alternative<std::vector<std::byte>>(data_);
    }

    /// Releases the owned buffer, or returns `std::nullopt` for a borrowed view.
    [[nodiscard]] std::optional<std::vector<std::byte>> release() &&
    {
        if (auto* owned = std::get_if<std::vector<std::byte>>(&data_))
            return std::move(*owned);

        return std::nullopt;
    }

private:
    std::variant<std::span<const std::byte>, std::vector<std::byte>> data_;
};
} // namespace ripper::pdf::core
