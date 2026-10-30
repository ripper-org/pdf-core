#pragma once

#include "ripper/pdf/core/document/revision_manager.hpp"

#include <memory>

namespace ripper::pdf::core
{
class document;

class revision_history_parser
{
public:
    virtual ~revision_history_parser() = default;

    /// Rebind this parser to `doc` after the owning document is moved.
    ///
    /// The default implementation is a no-op for implementations that do not
    /// retain a document reference.
    virtual void rebind(document& /*doc*/) noexcept {}

    virtual std::unique_ptr<revision_manager> parse() = 0;
};
} // namespace ripper::pdf::core
