#include "ripper/pdf/core/parser/header/header_parser.hpp"

#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"
#include "ripper/pdf/core/util/byte.hpp"

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace ripper::pdf::core
{
header_parser::header_parser(const document& document) : _document{&document} {}

void header_parser::rebind(const document& document) noexcept
{
    _document = &document;
}

header header_parser::parse()
{
    auto* _reader_ptr = _document->reader();
    if (_reader_ptr == nullptr)
        throw io_exception{"No reader backend available"};

    auto& _reader = *_reader_ptr;

    constexpr std::string_view kMagic = "%PDF-";
    constexpr std::size_t kMaxHeaderLineLength = 256;

    std::array<std::byte, kMaxHeaderLineLength> buffer{};

    _reader.seek(0);

    const std::size_t read = _reader.read_line(buffer);
    if (read == 0)
    {
        throw parse_exception{"File is empty while reading PDF header"};
    }

    const std::string_view line{byte::as_chars(buffer.data()), read};

    const std::size_t pos = line.find(kMagic);
    if (pos == std::string_view::npos)
    {
        throw parse_exception{"Missing PDF header signature"};
    }

    const std::string_view rest = line.substr(pos + kMagic.size());

    std::size_t len = 0;
    while (len < rest.size())
    {
        const unsigned char ch = static_cast<unsigned char>(rest[len]);
        if (!std::isdigit(ch) && ch != '.')
            break;
        ++len;
    }

    if (len == 0)
    {
        throw parse_exception{"Invalid PDF header version"};
    }

    const std::string_view version = rest.substr(0, len);
    const std::size_t dotPos = version.find('.');
    if (dotPos == std::string_view::npos || dotPos == 0 || dotPos + 1 >= version.size())
    {
        throw parse_exception{"Invalid PDF header version format"};
    }

    if (version.find('.', dotPos + 1) != std::string_view::npos)
    {
        throw parse_exception{"Invalid PDF header version format"};
    }

    return header{std::string{version}};
}
} // namespace ripper::pdf::core
