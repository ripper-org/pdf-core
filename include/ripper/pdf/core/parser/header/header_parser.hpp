#pragma once

#include "ripper/pdf/core/document.hpp"

namespace ripper::pdf::core
{
class header_parser
{
public:
    explicit header_parser(const document& document);

    /// Rebind this parser to `document` after the owning document is moved.
    void rebind(const document& document) noexcept;

    [[nodiscard]] header parse();

private:
    const document* _document;
};
} // namespace ripper::pdf::core
