#include "ripper/pdf/core/document/objstm.hpp"

#include "core/parser/object_stream_layout.hpp"
#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/object_stream_parser.hpp"
#include "ripper/pdf/core/util/checked.hpp"

#include <string>

namespace ripper::pdf::core
{

objstm::objstm(indirect_object& obj) noexcept : object_view(obj) {}

std::uint32_t objstm::count() const
{
    const auto* os = obj().content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object is not a stream"};

    return object_stream_count(os->dictionary(), obj().identity().owner().limits());
}

std::uint32_t objstm::first_offset() const
{
    const auto* os = obj().content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object is not a stream"};

    return checked::to_unsigned<std::uint32_t>(
        object_stream_first(os->dictionary(), os->raw().size()), "Object stream /First");
}

std::optional<class objstm> objstm::extension()
{
    auto* os = obj().content().as_stream();
    if (os == nullptr)
        return std::nullopt;

    const auto* ext_ref = os->dictionary().get_indirect_reference("Extends");
    if (ext_ref == nullptr)
        return std::nullopt;

    if (*ext_ref == obj().identity().reference())
        throw parse_exception{"Cyclic /Extends chain detected in object stream"};

    auto& doc = obj().identity().owner();
    auto* resolved = doc.resolve_object(*ext_ref);
    if (resolved == nullptr)
        return std::nullopt;

    return objstm{*resolved};
}

std::optional<objstm::object_range> objstm::object_offset(std::uint32_t index) const
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto* os = const_cast<objstm*>(this)->obj().content().as_stream();
    if (os == nullptr)
        return std::nullopt;

    const auto& limits = obj().identity().owner().limits();
    const auto layout = parse_object_stream_layout(os->raw(), os->dictionary(), limits);

    if (index >= layout.entries.size())
        return std::nullopt;

    const auto& entry = layout.entries[index];
    return object_range{entry.offset, entry.length};
}

std::vector<indirect_object> objstm::objects()
{
    auto& doc = obj().identity().owner();

    auto* os = obj().content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object is not a stream"};

    const auto* d = os->dictionary().get_name("Type");
    if (d == nullptr || d->value != "ObjStm")
        throw parse_exception{"Object is not an Object Stream (/Type /ObjStm)"};

    return object_stream_parser::parse(doc, *os);
}

} // namespace ripper::pdf::core
