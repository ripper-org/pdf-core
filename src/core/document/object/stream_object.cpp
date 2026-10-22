#include "ripper/pdf/core/document/object/stream_object.hpp"

#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/filter/filter_manager.hpp"

namespace ripper::pdf::core
{
/// Stream object implementation

stream_object::stream_object(class dictionary_object dict, class stream stream) noexcept
    : dict_(std::move(dict)), stream_(std::move(stream))
{
}

const dictionary_object& stream_object::dictionary() const noexcept
{
    return dict_;
}

dictionary_object& stream_object::dictionary() noexcept
{
    return dict_;
}

const stream& stream_object::stream() const noexcept
{
    return stream_;
}

stream& stream_object::stream() noexcept
{
    return stream_;
}

void stream_object::write(std::span<std::byte> in)
{
    stream_.write(in);
    sync_length();
}

void stream_object::set_length(std::size_t length)
{
    dict_.set("Length", object(static_cast<std::int64_t>(length)));
}

void stream_object::sync_length()
{
    const std::size_t length = stream_.size();
    dict_.set("Length", object(static_cast<std::int64_t>(length)));
}

bool stream_object::is_decoded() const noexcept
{
    return is_decoded_;
}

void stream_object::set_decoded(bool state) noexcept
{
    is_decoded_ = state;
}

std::span<const std::byte> stream_object::content(const resource_limits& limits)
{
    if (is_decoded_)
        return stream_.data();

    if (!dict_.contains("Filter"))
    {
        is_decoded_ = true;
        return stream_.data();
    }

    auto decoded = filter_manager::decode(dict_, stream_.data(), limits);
    stream_.data() = std::move(decoded);
    is_decoded_ = true;
    return stream_.data();
}

std::span<const std::byte> stream_object::raw() const noexcept
{
    return stream_.data();
}

} // namespace ripper::pdf::core
