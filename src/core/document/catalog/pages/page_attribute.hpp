#pragma once

#include "ripper/pdf/core/document/geometry/rect.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace ripper::pdf::core
{
class document;
class indirect_object;

/// Low-level helpers shared by the page tree views when reading inheritable
/// page attributes (/MediaBox, /CropBox, /Rotate, …).
namespace page_attribute
{
/// Normalizes `degrees` into the [0, 360) range.
[[nodiscard]] int normalize_rotation(std::int64_t degrees) noexcept;

/// Reads a four-number box in [llx lly urx ury] form. Returns `nullopt` when
/// the value is not a well-formed box.
[[nodiscard]] std::optional<geometry::rect> read_box(document& doc, const object& value);

/// Returns the named entry from `node`'s dictionary, resolving indirect
/// references. Returns `nullptr` when the entry is absent, null, or cannot be
/// resolved.
[[nodiscard]] const object* local_entry(indirect_object& node, std::string_view key);
} // namespace page_attribute
} // namespace ripper::pdf::core
