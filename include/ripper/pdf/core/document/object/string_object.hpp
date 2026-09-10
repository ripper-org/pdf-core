#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{

/// Represents a PDF string object with full round-trip provenance.
///
/// Tracks the encoding, the form used in the source (literal or hex),
/// the decoded byte payload, and the original raw bytes read from the file.
/// This enables lossless re-serialization: a hex string stays a hex string,
/// BOM-prefixed UTF-16BE strings are re-emitted with the BOM intact.
class string_object
{
public:
    /// Character encoding used by the string's payload.
    enum class encoding : std::uint8_t
    {
        utf16be,
        pdf_doc_encoding,
        utf8,
        identity
    };

    /// The serialized form the string was read in (or should be written in).
    enum class form : std::uint8_t
    {
        literal,
        hex
    };

    /// Construct from a decoded string, defaulting to literal form + UTF-8 encoding.
    /// The decoded bytes are also used as the original bytes.
    explicit string_object(std::string decoded);

    /// Construct with full provenance: decoded payload, original raw bytes,
    /// encoding, and serialized form.
    string_object(std::vector<std::byte> decoded_bytes, std::vector<std::byte> original_bytes,
                  encoding enc, form f) noexcept;

    /// Construct with a `string_view` convenience overload (decoded payload).
    string_object(std::string_view decoded, std::vector<std::byte> original_bytes, encoding enc,
                  form f);

    /// The decoded byte payload (unescaped, decoded from hex if applicable).
    [[nodiscard]] const std::vector<std::byte>& decoded_bytes() const noexcept;

    /// The original raw bytes read from the source file, exactly as they
    /// appeared between the delimiters.  Used for round-trip re-serialization.
    [[nodiscard]] const std::vector<std::byte>& original_bytes() const noexcept;

    /// Character encoding of the decoded payload.
    [[nodiscard]] encoding get_encoding() const noexcept;

    /// The serialized form (literal or hex) this string was read/written in.
    [[nodiscard]] form get_form() const noexcept;

    /// Returns `true` if the serialized form is hex (`<…>`).
    [[nodiscard]] bool is_hex() const noexcept;

    /// Returns `true` if the serialized form is literal (`(…)`).
    [[nodiscard]] bool is_literal() const noexcept;

    /// Returns the decoded bytes as a `std::string`.
    [[nodiscard]] std::string as_string() const;

    /// Returns the decoded bytes as a `std::string_view`.
    [[nodiscard]] std::string_view as_string_view() const noexcept;

    /// Equality comparison against another `string_object` (compares decoded bytes).
    bool operator==(const string_object& other) const noexcept;

    /// Equality comparison against a plain `std::string_view` (compares against decoded bytes).
    bool operator==(std::string_view sv) const;

private:
    std::vector<std::byte> decoded_bytes_;
    std::vector<std::byte> original_bytes_;
    encoding encoding_;
    form form_;
};

} // namespace ripper::pdf::core
