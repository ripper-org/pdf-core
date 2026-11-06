#include "ripper/pdf/core/document/object/object.hpp"

#include "ripper/pdf/core/document/object/dictionary_object.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/stream_object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <utility>

namespace ripper::pdf::core
{
/// Object implementation

object::object() noexcept : value_(null_object{}) {}

object::object(bool value) noexcept : value_(boolean_object{value}) {}

object::object(boolean_object value) noexcept : value_(std::move(value)) {}

object::object(std::int64_t value) noexcept : value_(number_object{value}) {}

object::object(double value) : value_(number_object{value}) {}

object::object(number_object value) noexcept : value_(std::move(value)) {}

object::object(std::string value) : value_(string_object{std::move(value)}) {}

object::object(string_object value) noexcept : value_(std::move(value)) {}

object::object(name_object value) noexcept : value_(std::move(value)) {}

object::object(array_object value) noexcept : value_(std::move(value)) {}

object::object(dictionary_object value) : value_(indirect<dictionary_object>{std::move(value)}) {}

object::object(stream_object value) : value_(indirect<stream_object>{std::move(value)}) {}

object::object(indirect_reference value) noexcept : value_(std::move(value)) {}

object::~object() = default;

object::object(object&& other) noexcept = default;

object& object::operator=(object&& other) noexcept = default;

object::object(const object& other) = default;

object& object::operator=(const object& other) = default;

bool object::operator==(const object& other) const noexcept = default;

object object::clone() const
{
    return object{*this};
}

bool object::is_null() const noexcept
{
    return std::holds_alternative<null_object>(value_);
}

bool object::is_boolean() const noexcept
{
    return std::holds_alternative<boolean_object>(value_);
}

bool object::is_number() const noexcept
{
    return std::holds_alternative<number_object>(value_);
}

bool object::is_integer() const noexcept
{
    const auto* num = std::get_if<number_object>(&value_);
    return num != nullptr && num->is_integer();
}

bool object::is_real() const noexcept
{
    const auto* num = std::get_if<number_object>(&value_);
    return num != nullptr && num->is_real();
}

bool object::is_string() const noexcept
{
    return std::holds_alternative<string_object>(value_);
}

bool object::is_name() const noexcept
{
    return std::holds_alternative<name_object>(value_);
}

bool object::is_array() const noexcept
{
    return std::holds_alternative<array_object>(value_);
}

bool object::is_dictionary() const noexcept
{
    return std::holds_alternative<indirect<dictionary_object>>(value_);
}

bool object::is_stream() const noexcept
{
    return std::holds_alternative<indirect<stream_object>>(value_);
}

bool object::is_indirect_reference() const noexcept
{
    return std::holds_alternative<indirect_reference>(value_);
}

const boolean_object* object::as_boolean() const noexcept
{
    return std::get_if<boolean_object>(&value_);
}

boolean_object* object::as_boolean() noexcept
{
    return std::get_if<boolean_object>(&value_);
}

const number_object* object::as_number() const noexcept
{
    return std::get_if<number_object>(&value_);
}

number_object* object::as_number() noexcept
{
    return std::get_if<number_object>(&value_);
}

const string_object* object::as_string() const noexcept
{
    return std::get_if<string_object>(&value_);
}

string_object* object::as_string() noexcept
{
    return std::get_if<string_object>(&value_);
}

const name_object* object::as_name() const noexcept
{
    return std::get_if<name_object>(&value_);
}

name_object* object::as_name() noexcept
{
    return std::get_if<name_object>(&value_);
}

const array_object* object::as_array() const noexcept
{
    return std::get_if<array_object>(&value_);
}

array_object* object::as_array() noexcept
{
    return std::get_if<array_object>(&value_);
}

const dictionary_object* object::as_dictionary() const noexcept
{
    const auto* ptr = std::get_if<indirect<dictionary_object>>(&value_);

    if (ptr != nullptr)
        return ptr->get();

    const auto* stream_ptr = std::get_if<indirect<stream_object>>(&value_);
    if (stream_ptr == nullptr)
        return nullptr;

    return stream_ptr->get() != nullptr ? &stream_ptr->get()->dictionary() : nullptr;
}

dictionary_object* object::as_dictionary() noexcept
{
    auto* ptr = std::get_if<indirect<dictionary_object>>(&value_);

    if (ptr != nullptr)
        return ptr->get();

    auto* stream_ptr = std::get_if<indirect<stream_object>>(&value_);
    if (stream_ptr == nullptr)
        return nullptr;

    return stream_ptr->get() != nullptr ? &stream_ptr->get()->dictionary() : nullptr;
}

const stream_object* object::as_stream() const noexcept
{
    const auto* ptr = std::get_if<indirect<stream_object>>(&value_);
    return ptr != nullptr ? ptr->get() : nullptr;
}

stream_object* object::as_stream() noexcept
{
    auto* ptr = std::get_if<indirect<stream_object>>(&value_);
    return ptr != nullptr ? ptr->get() : nullptr;
}

const indirect_reference* object::as_indirect_reference() const noexcept
{
    return std::get_if<indirect_reference>(&value_);
}

indirect_reference* object::as_indirect_reference() noexcept
{
    return std::get_if<indirect_reference>(&value_);
}

} // namespace ripper::pdf::core
