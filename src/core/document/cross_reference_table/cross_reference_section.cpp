#include "ripper/pdf/core/document/cross_reference_table/cross_reference_section.hpp"

#include "ripper/pdf/core/document/cross_reference_table/cross_reference_entry.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"

#include <memory>

namespace ripper::pdf::core
{
cross_reference_section::cross_reference_section(
    std::deque<cross_reference_subsection> subsections,
    std::optional<std::uint64_t> startxref_offset) noexcept
    : subsections_{std::move(subsections)}, startxref_offset_{startxref_offset}
{
}

cross_reference_entry* cross_reference_section::find(std::uint32_t object_number) noexcept
{
    for (auto& sub : subsections_)
    {
        if (auto* e = sub.find(object_number))
            return e;
    }

    return nullptr;
}

const cross_reference_entry*
cross_reference_section::find(std::uint32_t object_number) const noexcept
{
    for (const auto& sub : subsections_)
    {
        if (const auto* e = sub.find(object_number))
            return e;
    }

    return nullptr;
}

cross_reference_entry* cross_reference_section::find(const indirect_reference& ref) noexcept
{
    auto* entry = find(ref.object_number());
    if (entry != nullptr && entry->reference().generation() == ref.generation())
        return entry;

    return nullptr;
}

const cross_reference_entry*
cross_reference_section::find(const indirect_reference& ref) const noexcept
{
    const auto* entry = find(ref.object_number());
    if (entry != nullptr && entry->reference().generation() == ref.generation())
        return entry;

    return nullptr;
}

void cross_reference_section::add_entry(cross_reference_entry entry)
{
    const std::uint32_t num = entry.reference().object_number();

    if (!subsections_.empty())
    {
        auto& last = subsections_.back();
        const std::uint32_t expected_next = last.first_object_number() + last.count();
        if (num == expected_next)
        {
            last.entries().emplace(num, std::move(entry));
            return;
        }
    }

    cross_reference_subsection::entry_map new_entries;
    new_entries.emplace(num, std::move(entry));
    subsections_.emplace_back(num, std::move(new_entries));
}

indirect_reference cross_reference_section::reserve()
{
    if (auto slot = take_free_slot())
    {
        add_entry(cross_reference_entry{*slot});
        return *slot;
    }

    const std::uint32_t number = next_object_number();

    indirect_reference ref{number, 0};

    add_entry(cross_reference_entry{ref});

    return ref;
}

class indirect_object*
cross_reference_section::commit(const indirect_reference& ref,
                                std::unique_ptr<class indirect_object> object) noexcept
{
    auto* entry = find(ref);
    if (entry == nullptr)
        return nullptr;

    if (!entry->is_new() || entry->is_resolved())
        return nullptr;

    return entry->resolve(std::move(object));
}

indirect_reference cross_reference_section::allocate(std::unique_ptr<class indirect_object> object)
{
    if (auto slot = take_free_slot())
    {
        add_entry(cross_reference_entry{*slot, std::move(object)});
        return *slot;
    }

    const std::uint32_t number = next_object_number();

    indirect_reference ref{number, 0};

    add_entry(cross_reference_entry{ref, std::move(object)});

    return ref;
}

std::optional<indirect_reference> cross_reference_section::take_free_slot() noexcept
{
    auto* head = find(0);
    if (head == nullptr)
        return std::nullopt;

    const auto next = head->next_free_object();
    if (next == 0)
        return std::nullopt;

    auto* free_entry = find(next);
    if (free_entry == nullptr || free_entry->in_use())
        return std::nullopt;

    const auto recycled = indirect_reference{next, free_entry->reuse_generation()};
    const auto recycled_next = free_entry->next_free_object();

    for (auto& sub : subsections_)
    {
        if (sub.find(next) != nullptr)
        {
            sub.entries().erase(next);
            break;
        }
    }

    head->set_next_free_object(recycled_next);

    return recycled;
}

void cross_reference_section::link_free(std::uint32_t object_number) noexcept
{
    auto* head = find(0);
    auto* entry = find(object_number);
    if (head == nullptr || entry == nullptr || entry->in_use())
        return;

    entry->set_next_free_object(head->next_free_object());
    head->set_next_free_object(object_number);
}

void cross_reference_section::mark_deleted(const indirect_reference& ref) noexcept
{
    if (ref.object_number() == 0)
        return;

    auto* entry = find(ref);
    if (entry == nullptr)
        return;

    entry->mark_deleted();
    link_free(ref.object_number());
}

cross_reference_entry* cross_reference_section::add_entry_from(const cross_reference_entry& source)
{
    cross_reference_entry copy{source};
    auto ref = copy.reference();
    add_entry(std::move(copy));
    return find(ref);
}

std::map<std::uint32_t, cross_reference_entry*> cross_reference_section::entries()
{
    std::map<std::uint32_t, cross_reference_entry*> result;
    for (auto& sub : subsections_)
    {
        for (auto& [num, entry] : sub.entries())
            result.emplace(num, &entry);
    }

    return result;
}

std::size_t cross_reference_section::size() const noexcept
{
    std::size_t total = 0;
    for (const auto& sub : subsections_)
        total += sub.count();

    return total;
}

std::uint32_t cross_reference_section::next_object_number() const noexcept
{
    std::uint32_t max_num = 0;
    for (const auto& sub : subsections_)
    {
        for (const auto& [num, entry] : sub.entries())
            max_num = std::max(max_num, num);
    }

    return max_num + 1;
}

const std::deque<cross_reference_subsection>& cross_reference_section::subsections() const noexcept
{
    return subsections_;
}

std::deque<cross_reference_subsection>& cross_reference_section::subsections() noexcept
{
    return subsections_;
}

std::optional<std::uint64_t> cross_reference_section::startxref_offset() const noexcept
{
    return startxref_offset_;
}

void cross_reference_section::set_startxref_offset(std::uint64_t offset) noexcept
{
    startxref_offset_ = offset;
}

bool cross_reference_section::is_compressed() const noexcept
{
    return xref_stream_object_number_.has_value();
}

std::optional<std::uint32_t> cross_reference_section::xref_stream_object_number() const noexcept
{
    return xref_stream_object_number_;
}

void cross_reference_section::set_xref_stream_object_number(
    std::optional<std::uint32_t> object_number) noexcept
{
    xref_stream_object_number_ = object_number;
}
} // namespace ripper::pdf::core
