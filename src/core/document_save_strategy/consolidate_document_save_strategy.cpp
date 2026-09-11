#include "ripper/pdf/core/document_save_strategy/consolidate_document_save_strategy.hpp"

#include "ripper/io/core/writer/writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_manager.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/serializer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ripper::pdf::core
{

void consolidate_document_save_strategy::save(document& doc)
{
    if (!doc.has_writer())
        throw logic_exception{"No writer backend available"};

    if (!doc.has_serializer())
        throw logic_exception{"No serializer available"};

    doc.revisions().squash();

    doc.trailer().active_trailer().set_size(doc.cross_reference_table().next_object_number());

    auto& xref = doc.cross_reference_table().active_section();

    for (auto [number, entry_ptr] : xref.entries())
    {
        auto& entry = *entry_ptr;

        if (!entry.in_use())
            continue;

        if (!entry.is_resolved() && !entry.is_new())
            doc.resolve_object(entry.reference());
    }

    auto& w = *doc.writer();
    auto& s = *doc.serializer();

    auto serialized_header = s.serialize_header(doc.header());
    w.write(serialized_header);

    for (auto [number, entry_ptr] : xref.entries())
    {
        auto& entry = *entry_ptr;

        if (!entry.in_use())
            continue;

        auto* obj = entry.indirect_object();
        if (obj == nullptr)
            continue;

        entry.set_offset(static_cast<std::uint64_t>(w.tell()));

        w.write(s.serialize_indirect_object(*obj));
    }

    auto xref_start = static_cast<std::uint64_t>(w.tell());

    w.write(s.serialize_revision(doc.revisions().current(), xref_start));

    w.flush();
    w.close();
}

} // namespace ripper::pdf::core
