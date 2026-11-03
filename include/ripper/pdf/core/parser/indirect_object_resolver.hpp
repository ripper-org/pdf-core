#pragma once

#include "ripper/pdf/core/document/object/indirect_reference.hpp"

#include <string>

namespace ripper::io::core
{
class reader;
}

namespace ripper::pdf::core
{
class cross_reference_entry;
class document;

class indirect_object_resolver
{
public:
    explicit indirect_object_resolver(document& document);

    /// Rebind this resolver to `document` after the owning document is moved.
    void rebind(document& document) noexcept;

    [[nodiscard]] std::string resolve(indirect_reference ref) const;

private:
    document* document_;

    [[nodiscard]] std::string resolve_uncompressed(indirect_reference ref,
                                                   const cross_reference_entry& entry,
                                                   ripper::io::core::reader& r) const;

    [[nodiscard]] std::string resolve_compressed(indirect_reference ref,
                                                 const cross_reference_entry& entry) const;
};

} // namespace ripper::pdf::core
