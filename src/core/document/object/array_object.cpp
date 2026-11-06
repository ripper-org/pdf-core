#include "ripper/pdf/core/document/object/array_object.hpp"

#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"

namespace ripper::pdf::core
{

array_object::array_object(storage_type items) noexcept : items_(std::move(items)) {}

array_object::array_object(const array_object& other) : items_(other.items_) {}

array_object& array_object::operator=(const array_object& other)
{
    if (this != &other)
        items_ = other.items_;
    return *this;
}

array_object::~array_object() = default;

void array_object::push_back(object value)
{
    items_.push_back(std::move(value));
}

const object& array_object::operator[](std::size_t index) const
{
    return items_[index];
}

object& array_object::operator[](std::size_t index)
{
    return items_[index];
}

const object& array_object::at(std::size_t index) const
{
    return items_.at(index);
}

object& array_object::at(std::size_t index)
{
    return items_.at(index);
}

std::size_t array_object::size() const noexcept
{
    return items_.size();
}

bool array_object::empty() const noexcept
{
    return items_.empty();
}

const array_object::storage_type& array_object::items() const noexcept
{
    return items_;
}

array_object::storage_type& array_object::items() noexcept
{
    return items_;
}

array_object::storage_type::iterator array_object::begin() noexcept
{
    return items_.begin();
}

array_object::storage_type::const_iterator array_object::begin() const noexcept
{
    return items_.begin();
}

array_object::storage_type::iterator array_object::end() noexcept
{
    return items_.end();
}

array_object::storage_type::const_iterator array_object::end() const noexcept
{
    return items_.end();
}

array_object::storage_type::iterator array_object::erase(storage_type::const_iterator it)
{
    return items_.erase(it);
}

bool array_object::operator==(const array_object& other) const noexcept
{
    if (items_.size() != other.items_.size())
        return false;

    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        if (!(items_[i] == other.items_[i]))
            return false;
    }

    return true;
}

} // namespace ripper::pdf::core
