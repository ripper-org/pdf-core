#include "ripper/pdf/core/document/catalog/pages/page/content/page_contents.hpp"

#include <utility>

namespace ripper::pdf::core
{
page_contents::page_contents(storage_type streams, bool is_array) noexcept
    : streams_(std::move(streams)), is_array_(is_array)
{
}
} // namespace ripper::pdf::core
