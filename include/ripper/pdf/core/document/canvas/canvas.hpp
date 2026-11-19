#pragma once

#include "ripper/pdf/core/document/object/object.hpp"

namespace ripper::pdf::core
{
enum class canvas_mode
{
    append,
    prepend,
    replace
};

class canvas
{
public:
    explicit canvas(stream_object& stream) noexcept;

private:
    stream_object& stream_;
};
} // namespace ripper::pdf::core
