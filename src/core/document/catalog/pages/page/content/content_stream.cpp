#include "ripper/pdf/core/document/catalog/pages/page/content/content_stream.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/object_manager.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <utility>

namespace ripper::pdf::core
{
content_stream::content_stream(indirect_object& obj) noexcept : indirect_object_view(obj) {}

content_stream content_stream::create(document& doc)
{
    // An empty stream still needs /Length in its dictionary: the serializer
    // writes the dictionary verbatim.
    stream_object stream_obj;
    stream_obj.sync_length();

    return content_stream{object_manager::allocate_stream(doc, std::move(stream_obj))};
}

stream_object* content_stream::stream() noexcept
{
    return obj().content().as_stream();
}

const stream_object* content_stream::stream() const noexcept
{
    return obj().content().as_stream();
}

instruction_stream content_stream::instructions()
{
    auto* content = stream();
    if (content == nullptr)
        throw logic_exception{"Content stream object is not a stream"};

    return instruction_stream{*content, obj().identity().owner().resource_limits()};
}
} // namespace ripper::pdf::core
