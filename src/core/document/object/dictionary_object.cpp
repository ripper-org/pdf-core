#include "ripper/pdf/core/document/object/dictionary_object.hpp"

#include "ripper/pdf/core/document/object/object.hpp"

namespace ripper::pdf::core
{
/// Dictionary object implementation

dictionary_object::dictionary_object(dictionary_map_type entries) noexcept
    : entries_(std::move(entries))
{
}

dictionary_object& dictionary_object::set(std::string key, object value)
{
    entries_.insert_or_assign(std::move(key), std::move(value));
    return *this;
}

dictionary_object& dictionary_object::remove(const std::string& key)
{
    entries_.erase(key);
    return *this;
}

bool dictionary_object::contains(const std::string& key) const noexcept
{
    return entries_.contains(key);
}

std::size_t dictionary_object::size() const noexcept
{
    return entries_.size();
}

bool dictionary_object::empty() const noexcept
{
    return entries_.empty();
}

const object* dictionary_object::get(const std::string& key) const noexcept
{
    const auto it = entries_.find(key);
    return it != entries_.end() ? &it->second : nullptr;
}

object* dictionary_object::get(const std::string& key) noexcept
{
    const auto it = entries_.find(key);
    return it != entries_.end() ? &it->second : nullptr;
}

const boolean_object* dictionary_object::get_boolean(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_boolean() : nullptr;
}

boolean_object* dictionary_object::get_boolean(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_boolean() : nullptr;
}

const number_object* dictionary_object::get_number(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    if (obj == nullptr)
        return nullptr;
    return obj->as_number();
}

number_object* dictionary_object::get_number(const std::string& key) noexcept
{
    auto* obj = get(key);
    if (obj == nullptr)
        return nullptr;
    return obj->as_number();
}

const string_object* dictionary_object::get_string(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_string() : nullptr;
}

string_object* dictionary_object::get_string(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_string() : nullptr;
}

const name_object* dictionary_object::get_name(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_name() : nullptr;
}

name_object* dictionary_object::get_name(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_name() : nullptr;
}

const array_object* dictionary_object::get_array(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_array() : nullptr;
}

array_object* dictionary_object::get_array(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_array() : nullptr;
}

const dictionary_object* dictionary_object::get_dictionary(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_dictionary() : nullptr;
}

dictionary_object* dictionary_object::get_dictionary(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_dictionary() : nullptr;
}

const indirect_reference*
dictionary_object::get_indirect_reference(const std::string& key) const noexcept
{
    const auto* obj = get(key);
    return obj != nullptr ? obj->as_indirect_reference() : nullptr;
}

indirect_reference* dictionary_object::get_indirect_reference(const std::string& key) noexcept
{
    auto* obj = get(key);
    return obj != nullptr ? obj->as_indirect_reference() : nullptr;
}

const dictionary_object::dictionary_map_type& dictionary_object::entries() const noexcept
{
    return entries_;
}

dictionary_object::dictionary_map_type& dictionary_object::entries() noexcept
{
    return entries_;
}

bool dictionary_object::operator==(const dictionary_object& other) const noexcept
{
    if (entries_.size() != other.entries_.size())
        return false;

    for (const auto& [key, value] : entries_)
    {
        const auto it = other.entries_.find(key);

        if (it == other.entries_.end() || !(value == it->second))
            return false;
    }

    return true;
}

} // namespace ripper::pdf::core
