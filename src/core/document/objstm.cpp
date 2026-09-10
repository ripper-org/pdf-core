#include "ripper/pdf/core/document/objstm.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/parser/object_stream_parser.hpp"
#include "ripper/pdf/core/util/byte.hpp"

#include <charconv>
#include <string>
#include <vector>

namespace ripper::pdf::core
{

objstm::objstm(indirect_object& obj) noexcept : object_view(obj) {}

std::uint32_t objstm::count() const
{
    const auto* os = obj().content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object is not a stream"};

    const auto* n = os->dictionary().get_number("N");
    if (n == nullptr)
        throw parse_exception{"Object Stream missing required /N"};

    return static_cast<std::uint32_t>(n->as_integer());
}

std::uint32_t objstm::first_offset() const
{
    const auto* os = obj().content().as_stream();
    if (os == nullptr)
        throw parse_exception{"Object is not a stream"};

    const auto* first = os->dictionary().get_number("First");
    if (first == nullptr)
        throw parse_exception{"Object Stream missing required /First"};

    return static_cast<std::uint32_t>(first->as_integer());
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
    if (index >= count())
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto* os = const_cast<objstm*>(this)->obj().content().as_stream();
    if (os == nullptr)
        return std::nullopt;

    auto content = os->raw();
    const auto n = count();
    const auto first = first_offset();

    // Parse all header entries to collect absolute byte positions.
    // Header format: N pairs of (object_number byte_offset).
    std::vector<std::size_t> positions;
    positions.reserve(n);

    std::size_t pos = 0;
    for (std::uint32_t i = 0; i < n; ++i)
    {
        pos = byte::skip_whitespace(content, pos, first);
        pos = byte::skip_non_whitespace(content, pos, first);
        pos = byte::skip_whitespace(content, pos, first);

        auto off_start = pos;
        pos = byte::skip_non_whitespace(content, pos, first);

        std::string off_str;
        off_str.reserve(pos - off_start);
        for (std::size_t j = off_start; j < pos; ++j)
            off_str += static_cast<char>(content[j]);

        std::size_t byte_offset = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::from_chars(off_str.data(), off_str.data() + off_str.size(), byte_offset);

        positions.push_back(first + byte_offset);
    }

    const auto obj_start = positions[index];
    const auto obj_end = (index + 1 < n) ? positions[index + 1] : content.size();

    if (obj_start >= content.size() || obj_end > content.size())
        return std::nullopt;

    const auto len = obj_end > obj_start ? obj_end - obj_start : 0;

    return object_range{obj_start, len};
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
