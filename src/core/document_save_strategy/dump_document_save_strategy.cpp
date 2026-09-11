#include "ripper/pdf/core/document_save_strategy/dump_document_save_strategy.hpp"

#include "ripper/io/core/writer/writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/serializer.hpp"

#include <cstddef>
#include <cstdint>

namespace ripper::pdf::core
{

void dump_document_save_strategy::save(document& doc)
{
    if (!doc.has_writer())
        throw logic_exception{"No writer backend available"};

    if (!doc.has_serializer())
        throw logic_exception{"No serializer available"};

    auto& revisions = doc.revisions().all();

    if (revisions.empty())
        throw logic_exception{"No revisions available"};

    for (auto& rev : revisions)
    {
        for (auto [number, entry_ptr] : rev.section().entries())
        {
            auto& entry = *entry_ptr;

            if (!entry.in_use())
                continue;

            if (!entry.is_resolved() && !entry.is_new())
                doc.resolve_object(entry.reference());
        }
    }

    auto& w = *doc.writer();
    auto& s = *doc.serializer();

    auto serialized_header = s.serialize_header(doc.header());
    w.write(serialized_header);

    for (auto& rev : revisions)
    {
        for (auto [number, entry_ptr] : rev.section().entries())
        {
            auto& entry = *entry_ptr;

            auto* obj = entry.indirect_object();
            if (obj == nullptr)
                continue;

            entry.set_offset(static_cast<std::uint64_t>(w.tell()));

            w.write(s.serialize_indirect_object(*obj));
        }

        auto xref_start = static_cast<std::uint64_t>(w.tell());

        w.write(s.serialize_revision(rev, xref_start));
    }

    w.flush();
    w.close();
}

} // namespace ripper::pdf::core
