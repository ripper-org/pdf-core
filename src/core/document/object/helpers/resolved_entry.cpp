#include "ripper/pdf/core/document/object/helpers/resolved_entry.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"

#include <string>
#include <string_view>

namespace ripper::pdf::core
{
const object* resolved_entry(indirect_object& node, std::string_view key)
{
    const auto* dict = node.content().as_dictionary();
    if (dict == nullptr)
        return nullptr;

    const auto* value = dict->get(std::string{key});
    if (value == nullptr)
        return nullptr;

    if (const auto* ref = value->as_indirect_reference())
    {
        auto* resolved = node.identity().owner().resolve_object(*ref);
        if (resolved == nullptr || resolved->content().is_null())
            return nullptr;

        return &resolved->content();
    }

    if (value->is_null())
        return nullptr;

    return value;
}
} // namespace ripper::pdf::core
