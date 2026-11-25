#pragma once

#include <string_view>

namespace ripper::pdf::core
{
class indirect_object;
class object;

/// Returns the named entry from `node`'s own dictionary, resolving indirect
/// references through the owning document.
///
/// Only this node is consulted; inheritance is the caller's concern.
///
/// Returns `nullptr` when the entry is absent, holds /Null, or `node` is not a
/// dictionary. A reference that cannot be resolved throws `parse_exception`.
[[nodiscard]] const object* resolved_entry(indirect_object& node, std::string_view key);
} // namespace ripper::pdf::core
