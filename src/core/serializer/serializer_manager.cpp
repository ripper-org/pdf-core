#include "ripper/pdf/core/serializer/serializer_manager.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/compressed_cross_reference_table_serializer.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/default_cross_reference_table_serializer.hpp"
#include "ripper/pdf/core/serializer/header/default_header_serializer.hpp"
#include "ripper/pdf/core/serializer/indirect_object/default_indirect_object_serializer.hpp"
#include "ripper/pdf/core/serializer/object/default_object_serializer.hpp"
#include "ripper/pdf/core/serializer/revision_serializer.hpp"
#include "ripper/pdf/core/serializer/trailer/default_trailer_serializer.hpp"

#include <memory>
#include <utility>

namespace ripper::pdf::core
{
serializer_manager::serializer_manager(const document& doc) : document_{&doc} {}

void serializer_manager::rebind(const document& doc) noexcept
{
    document_ = &doc;
}

void serializer_manager::set_header_serializer(std::unique_ptr<class header_serializer> object)
{
    header_serializer_ = std::move(object);
}

void serializer_manager::set_object_serializer(std::unique_ptr<class object_serializer> object)
{
    object_serializer_ = std::move(object);
    indirect_object_serializer().set_object_serializer(object_serializer());
    trailer_serializer().set_object_serializer(object_serializer());

    // If a compressed cross-reference serializer has been injected, also
    // propagate the object serializer so its dictionary_object serialization works.
    if (cross_reference_table_serializer_ != nullptr)
    {
        auto* compressed = dynamic_cast<class compressed_cross_reference_table_serializer*>(
            cross_reference_table_serializer_.get());
        if (compressed != nullptr)
            compressed->set_object_serializer(object_serializer());
    }
}

void serializer_manager::set_indirect_object_serializer(
    std::unique_ptr<class indirect_object_serializer> object)
{
    indirect_object_serializer_ = std::move(object);
}

void serializer_manager::set_cross_reference_table_serializer(
    std::unique_ptr<class cross_reference_table_serializer> object)
{
    cross_reference_table_serializer_ = std::move(object);

    // If a compressed serializer was injected and an object serializer is
    // already configured, wire it up so dictionary_object serialization is available.
    auto* compressed = dynamic_cast<class compressed_cross_reference_table_serializer*>(
        cross_reference_table_serializer_.get());
    if (compressed != nullptr && object_serializer_ != nullptr)
        compressed->set_object_serializer(*object_serializer_);

    // The revision_serializer was constructed with the previous xref serializer;
    // discard it so the next access rebuilds with the new one.
    revision_serializer_.reset();
}

void serializer_manager::set_trailer_serializer(std::unique_ptr<class trailer_serializer> object)
{
    trailer_serializer_ = std::move(object);
    revision_serializer_.reset();
}

void serializer_manager::set_line_break_character(char c)
{
    header_serializer().set_line_break_character(c);
    object_serializer().set_line_break_character(c);
    indirect_object_serializer().set_line_break_character(c);
    trailer_serializer().set_line_break_character(c);
}

void serializer_manager::set_object_break_character(char c)
{
    object_serializer().set_object_break_character(c);
    indirect_object_serializer().set_object_break_character(c);
}

header_serializer& serializer_manager::header_serializer()
{
    if (header_serializer_ == nullptr)
        header_serializer_ = std::make_unique<class default_header_serializer>();

    return *header_serializer_;
}

object_serializer& serializer_manager::object_serializer()
{
    if (object_serializer_ == nullptr)
        object_serializer_ = std::make_unique<class default_object_serializer>();

    return *object_serializer_;
}

indirect_object_serializer& serializer_manager::indirect_object_serializer()
{
    if (indirect_object_serializer_ == nullptr)
        indirect_object_serializer_ =
            std::make_unique<class default_indirect_object_serializer>(object_serializer());

    return *indirect_object_serializer_;
}

cross_reference_table_serializer& serializer_manager::cross_reference_table_serializer()
{
    if (cross_reference_table_serializer_ == nullptr)
        cross_reference_table_serializer_ =
            std::make_unique<class default_cross_reference_table_serializer>();

    return *cross_reference_table_serializer_;
}

trailer_serializer& serializer_manager::trailer_serializer()
{
    if (trailer_serializer_ == nullptr)
        trailer_serializer_ =
            std::make_unique<class default_trailer_serializer>(object_serializer());

    return *trailer_serializer_;
}

revision_serializer& serializer_manager::revision_serializer()
{
    if (revision_serializer_ == nullptr)
        revision_serializer_ = std::make_unique<class revision_serializer>(
            cross_reference_table_serializer(), trailer_serializer());

    return *revision_serializer_;
}
} // namespace ripper::pdf::core
