#include "ripper/pdf/core/content/content_stream.hpp"

#include <stdexcept>
#include <utility>

namespace ripper::pdf::core
{
content_stream::content_stream(storage_type operations) noexcept
    : operations_(std::move(operations))
{
}

void content_stream::append(content_operation operation)
{
    operations_.push_back(std::move(operation));
}

void content_stream::insert(std::size_t index, content_operation operation)
{
    if (index > operations_.size())
    {
        throw std::out_of_range{"content_stream::insert index out of range"};
    }

    operations_.insert(operations_.begin() + static_cast<std::ptrdiff_t>(index),
                       std::move(operation));
}

void content_stream::erase(std::size_t index)
{
    if (index >= operations_.size())
    {
        throw std::out_of_range{"content_stream::erase index out of range"};
    }

    operations_.erase(operations_.begin() + static_cast<std::ptrdiff_t>(index));
}

content_operation& content_stream::at(std::size_t index)
{
    return operations_.at(index);
}

const content_operation& content_stream::at(std::size_t index) const
{
    return operations_.at(index);
}

content_operation& content_stream::operator[](std::size_t index) noexcept
{
    return operations_[index];
}

const content_operation& content_stream::operator[](std::size_t index) const noexcept
{
    return operations_[index];
}

std::size_t content_stream::size() const noexcept
{
    return operations_.size();
}

bool content_stream::empty() const noexcept
{
    return operations_.empty();
}

void content_stream::clear() noexcept
{
    operations_.clear();
}

content_stream::storage_type& content_stream::operations() noexcept
{
    return operations_;
}

const content_stream::storage_type& content_stream::operations() const noexcept
{
    return operations_;
}

content_stream::iterator content_stream::begin() noexcept
{
    return operations_.begin();
}

content_stream::const_iterator content_stream::begin() const noexcept
{
    return operations_.begin();
}

content_stream::iterator content_stream::end() noexcept
{
    return operations_.end();
}

content_stream::const_iterator content_stream::end() const noexcept
{
    return operations_.end();
}

content_stream::const_iterator content_stream::cbegin() const noexcept
{
    return operations_.cbegin();
}

content_stream::const_iterator content_stream::cend() const noexcept
{
    return operations_.cend();
}
} // namespace ripper::pdf::core
