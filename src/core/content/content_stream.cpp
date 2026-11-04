#include "ripper/pdf/core/content/content_stream.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
void insert_item(content_stream::container_type& items, std::size_t index, content_item item)
{
    if (index > items.size())
    {
        throw std::out_of_range{"content_stream::insert index out of range"};
    }

    items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), std::move(item));
}
} // namespace

content_stream::content_stream(container_type items) noexcept : items_(std::move(items)) {}

content_stream::content_stream(std::vector<content_operation> operations) noexcept
{
    items_.reserve(operations.size());

    for (auto& operation : operations)
    {
        items_.emplace_back(std::move(operation));
    }
}

void content_stream::append(content_operation operation)
{
    items_.emplace_back(std::move(operation));
}

void content_stream::append(inline_image image)
{
    items_.emplace_back(std::move(image));
}

void content_stream::insert(std::size_t index, content_operation operation)
{
    insert_item(items_, index, content_item{std::move(operation)});
}

void content_stream::insert(std::size_t index, inline_image image)
{
    insert_item(items_, index, content_item{std::move(image)});
}

void content_stream::erase(std::size_t index)
{
    if (index >= items_.size())
    {
        throw std::out_of_range{"content_stream::erase index out of range"};
    }

    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
}

content_item& content_stream::at(std::size_t index)
{
    return items_.at(index);
}

const content_item& content_stream::at(std::size_t index) const
{
    return items_.at(index);
}

content_item& content_stream::operator[](std::size_t index) noexcept
{
    return items_[index];
}

const content_item& content_stream::operator[](std::size_t index) const noexcept
{
    return items_[index];
}

std::size_t content_stream::size() const noexcept
{
    return items_.size();
}

bool content_stream::empty() const noexcept
{
    return items_.empty();
}

std::size_t content_stream::operation_count() const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(items_.begin(), items_.end(), [](const content_item& item)
                      { return std::holds_alternative<content_operation>(item); }));
}

std::size_t content_stream::inline_image_count() const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(items_.begin(), items_.end(), [](const content_item& item)
                      { return std::holds_alternative<inline_image>(item); }));
}

void content_stream::clear() noexcept
{
    items_.clear();
}

content_stream::container_type& content_stream::items() noexcept
{
    return items_;
}

const content_stream::container_type& content_stream::items() const noexcept
{
    return items_;
}

content_stream::iterator content_stream::begin() noexcept
{
    return items_.begin();
}

content_stream::const_iterator content_stream::begin() const noexcept
{
    return items_.begin();
}

content_stream::iterator content_stream::end() noexcept
{
    return items_.end();
}

content_stream::const_iterator content_stream::end() const noexcept
{
    return items_.end();
}

content_stream::const_iterator content_stream::cbegin() const noexcept
{
    return items_.cbegin();
}

content_stream::const_iterator content_stream::cend() const noexcept
{
    return items_.cend();
}
} // namespace ripper::pdf::core
