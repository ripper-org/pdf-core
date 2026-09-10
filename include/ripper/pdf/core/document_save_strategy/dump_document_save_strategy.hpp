#pragma once

#include "ripper/pdf/core/document_save_strategy/document_save_strategy.hpp"

namespace ripper::pdf::core
{

/// In-memory dump document save strategy.
///
/// Writes every in-memory object exactly as it lives in the document, without
/// validation, correction, or squash.  Corrupted objects, broken references,
/// or invalid dictionary_object structures are serialised verbatim. It is the caller's
/// responsibility to ensure correctness.
///
/// The only mechanical step is a resolve pass to load any on-disk objects that
/// have not yet been brought into memory; after that, each xref section and
/// its corresponding trailer are written as-is, preserving the original
/// incremental-update structure.
class dump_document_save_strategy final : public document_save_strategy
{
public:
    void save(document& doc) override;
};

} // namespace ripper::pdf::core
