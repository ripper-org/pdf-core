#include "ripper/pdf/core/document/canvas/canvas.hpp"

namespace ripper::pdf::core
{
canvas::canvas(stream_object& stream) noexcept : stream_(stream) {}
} // namespace ripper::pdf::core
