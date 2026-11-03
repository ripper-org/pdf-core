#pragma once

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/serializer/cross_reference_table/cross_reference_table_serializer.hpp"
#include "ripper/pdf/core/serializer/header/header_serializer.hpp"
#include "ripper/pdf/core/serializer/indirect_object/indirect_object_serializer.hpp"
#include "ripper/pdf/core/serializer/object/object_serializer.hpp"
#include "ripper/pdf/core/serializer/revision_serializer.hpp"
#include "ripper/pdf/core/serializer/trailer/trailer_serializer.hpp"

#include <memory>

namespace ripper::pdf::core
{
class revision_serializer;

/// Owns and exposes the serializer subcomponents used to process a `document`.
///
/// This type centralizes serializer dependencies and enables runtime injection
/// of concrete serializer implementations (useful for composition and testing).
/// All injected components are owned via `std::unique_ptr`.
class serializer_manager
{
public:
    /// Construct a serializer manager bound to `doc`.
    ///
    /// The serializer manager stores a reference and does not take ownership of the document.
    explicit serializer_manager(const document& doc);

    /// Rebind this manager to `doc` after the owning document is moved.
    ///
    /// Serializer subcomponents receive the document per call and therefore
    /// require no rebinding.
    void rebind(const document& doc) noexcept;

    /// Replace the header serializer implementation.
    void set_header_serializer(std::unique_ptr<class header_serializer> object);

    /// Replace the object value serializer.
    ///
    /// Also propagates to the indirect_object_serializer, the trailer_serializer,
    /// and any injected `compressed_cross_reference_table_serializer` so all stay in sync.
    void set_object_serializer(std::unique_ptr<class object_serializer> object);

    /// Replace the indirect-object serializer implementation.
    void set_indirect_object_serializer(std::unique_ptr<class indirect_object_serializer> object);

    /// Replace the cross-reference table serializer implementation.
    ///
    /// When the injected serializer is a `compressed_cross_reference_table_serializer`,
    /// the currently-configured object serializer (if any) is automatically propagated
    /// to it so its dictionary_object serialization has the dependencies it needs.
    void set_cross_reference_table_serializer(
        std::unique_ptr<class cross_reference_table_serializer> object);

    /// Replace the trailer serializer implementation.
    void set_trailer_serializer(std::unique_ptr<class trailer_serializer> object);

    /// Propagate a line-break character to all sub-serializers.
    void set_line_break_character(char c);

    /// Propagate an object-break character to all sub-serializers that use it.
    void set_object_break_character(char c);

    /// Access the configured header serializer.
    [[nodiscard]] class header_serializer& header_serializer();

    /// Access the configured object serializer.
    [[nodiscard]] class object_serializer& object_serializer();

    /// Access the configured indirect-object serializer.
    [[nodiscard]] class indirect_object_serializer& indirect_object_serializer();

    /// Access the configured cross-reference table serializer.
    [[nodiscard]] class cross_reference_table_serializer& cross_reference_table_serializer();

    /// Access the configured trailer serializer.
    [[nodiscard]] class trailer_serializer& trailer_serializer();

    /// Access the revision serializer.
    ///
    /// The revision serializer is created lazily on first access using the currently
    /// configured cross-reference-table and trailer serializers. It orchestrates the
    /// serialization of a single PDF revision (compressed or traditional).
    [[nodiscard]] class revision_serializer& revision_serializer();

private:
    const document* document_;

    std::unique_ptr<class header_serializer> header_serializer_;
    std::unique_ptr<class object_serializer> object_serializer_;
    std::unique_ptr<class indirect_object_serializer> indirect_object_serializer_;
    std::unique_ptr<class cross_reference_table_serializer> cross_reference_table_serializer_;
    std::unique_ptr<class trailer_serializer> trailer_serializer_;
    std::unique_ptr<class revision_serializer> revision_serializer_;
};
} // namespace ripper::pdf::core
