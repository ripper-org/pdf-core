#pragma once

#include "ripper/pdf/core/document/object/stream_object.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ripper::pdf::core::test_support
{
inline std::string content_of(const stream_object& stream)
{
    const auto& bytes = stream.stream().data();
    std::string text;
    text.reserve(bytes.size());

    for (const std::byte byte : bytes)
        text.push_back(static_cast<char>(byte));

    return text;
}

inline std::size_t count_of(std::string_view haystack, std::string_view needle)
{
    std::size_t count = 0;

    for (auto position = haystack.find(needle); position != std::string_view::npos;
         position = haystack.find(needle, position + needle.size()))
    {
        ++count;
    }

    return count;
}
} // namespace ripper::pdf::core::test_support
