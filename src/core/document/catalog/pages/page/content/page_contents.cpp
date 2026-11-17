#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include <utility>

namespace ripper::pdf::core
{
page_contents::page_contents(std::vector<content_stream> streams) noexcept
    : streams_(std::move(streams))
{}

bool page_contents::empty() const noexcept
{
    return streams_.empty();
}

bool page_contents::is_multiple() const noexcept
{
    return streams_.size() > 1;
}

std::span<const content_stream> page_contents::streams() const noexcept
{
    return streams_;
}

} // namespace ripper::pdf::core
