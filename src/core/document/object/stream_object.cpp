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

decoded_stream stream_object::decode(const resource_limits& limits) const
{
    // Unfiltered streams are already in decoded form; borrow the stored bytes.
    if (!dict_.contains("Filter"))
        return decoded_stream{raw()};

    return decoded_stream{filter_manager::decode(dict_, raw(), limits)};
}

std::span<const std::byte> stream_object::raw() const noexcept
{
    return stream_.data();
}

bool stream_object::operator==(const stream_object& other) const noexcept
{
    if (!(dict_ == other.dict_))
        return false;

    const auto& lhs = stream_.data();
    const auto& rhs = other.stream_.data();

    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

} // namespace ripper::pdf::core
