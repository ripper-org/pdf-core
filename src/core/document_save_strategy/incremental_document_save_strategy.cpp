#include "ripper/pdf/core/document_save_strategy/incremental_document_save_strategy.hpp"

#include "ripper/io/core/reader/reader.hpp"
#include "ripper/io/core/writer/writer.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/revision.hpp"
#include "ripper/pdf/core/document/revision_manager.hpp"
#include "ripper/pdf/core/document/trailer/trailer.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/serializer/serializer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ripper::pdf::core
{

void incremental_document_save_strategy::save(document& doc)
{
    if (!doc.has_reader())
        throw logic_exception{
            "No reader backend available. Incremental save requires the original file"};

    if (!doc.has_writer())
        throw logic_exception{"No writer backend available"};

    if (!doc.has_serializer())
        throw logic_exception{"No serializer available"};

    auto& revisions = doc.revisions().all();

    if (revisions.empty())
        throw logic_exception{"No revisions available"};

    auto& r = *doc.reader();
    auto& w = *doc.writer();
    auto& s = *doc.serializer();

    constexpr std::size_t k_copy_buf_size = 1u << 20;
    r.seek(0);

    // 1 MiB array on the stack overflows MSVC's default 1 MiB
    // stack at function entry (Linux/macOS default to 8 MiB).
    std::vector<std::byte> copy_buf(k_copy_buf_size);

    while (true)
    {
        auto bytes_read = r.read(copy_buf);
        if (bytes_read == 0)
            break;

        w.write(std::span{copy_buf.data(), bytes_read});
    }

    std::optional<std::uint64_t> prev_xref_start;

    for (auto it = revisions.rbegin(); it != revisions.rend(); ++it)
    {
        if (it->section().startxref_offset().has_value())
        {
            prev_xref_start = it->section().startxref_offset();
            break;
        }
    }

    for (std::size_t i = 0; i < revisions.size(); ++i)
    {
        auto& rev = revisions[i];

        if (rev.section().startxref_offset().has_value())
            continue;

        for (auto [number, entry_ptr] : rev.section().entries())
        {
            auto& entry = *entry_ptr;

            if (!entry.in_use())
                continue;

            if (!entry.is_resolved() && !entry.is_new())
                doc.resolve_object(entry.reference());
        }

        for (auto [number, entry_ptr] : rev.section().entries())
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

        if (prev_xref_start.has_value())
            rev.trailer().dictionary().set("Prev",
                                           object{static_cast<std::int64_t>(*prev_xref_start)});
        else
            rev.trailer().dictionary().remove("Prev");

        const auto* prev_id_arr =
            (i > 0) ? revisions[i - 1].trailer().dictionary().get_array("ID") : nullptr;

        if (prev_id_arr != nullptr && !prev_id_arr->empty())
        {
            array_object new_id;
            new_id.push_back((*prev_id_arr)[0]);

            auto new_current =
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

            new_id.push_back(object{std::move(new_current)});

            rev.trailer().dictionary().set("ID", object{std::move(new_id)});
        }

        w.write(s.serialize_revision(rev, xref_start));
        rev.section().set_startxref_offset(xref_start);

        prev_xref_start = xref_start;
    }

    w.flush();
    w.close();
}

} // namespace ripper::pdf::core
