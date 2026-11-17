#pragma once

#include "ripper/pdf/core/document/object/object.hpp"

namespace ripper::pdf::core
{
class canvas
{
    public:
        explicit canvas(stream_object& stream) noexcept;
    private:
        stream_object& stream_;
};
}
