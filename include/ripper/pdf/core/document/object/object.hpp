#pragma once

#include "ripper/pdf/core/document/object/array_object.hpp"
#include "ripper/pdf/core/document/object/boolean_object.hpp"
#include "ripper/pdf/core/document/object/helpers/stream.hpp"
#include "ripper/pdf/core/document/object/indirect_reference.hpp"
#include "ripper/pdf/core/document/object/name_object.hpp"
#include "ripper/pdf/core/document/object/null_object.hpp"
#include "ripper/pdf/core/document/object/number_object.hpp"
#include "ripper/pdf/core/document/object/string_object.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>

namespace ripper::pdf::core
{

class dictionary_object;
class stream_object;

/// A PDF dictionary object. An associative map of name keys to `object` objects.
///
/// Wraps the raw `unordered_map<string, object>` representation with typed
/// accessors and lookup helpers to reduce boilerplate in parsing and traversal code.
///
/// ## Key convention
///
/// Keys are stored without the leading `/` (e.g. `"Type"`, not `"/Type"`).
/// This matches how parsed PDF names should be normalized before insertion.
///
/// ## Access patterns
///
/// Raw access via `get()` returns a pointer to the held `object`, or `nullptr`
/// if the key is absent. Typed accessors (`get_name`, `get_number`, etc.) combine
/// the lookup and type-check into a single step, returning `nullptr` when either
/// the key is missing or the value holds a different type.
///
/// When full traversal is needed, `entries()` exposes the underlying map directly.
class dictionary_object
{
public:
    /// Type alias for the underlying storage.
    using dictionary_map_type = std::unordered_map<std::string, object>;

    /// Construct an empty dictionary.
    dictionary_object() noexcept = default;

    /// Construct a dictionary from an existing map.
    explicit dictionary_object(dictionary_map_type entries) noexcept;

    /// Insert or overwrite a key-value pair.
    dictionary_object& set(std::string key, object value);

    /// Remove an entry by key.
    dictionary_object& remove(const std::string& key);

    /// Returns `true` if the dictionary contains the given key.
    [[nodiscard]] bool contains(const std::string& key) const noexcept;

    /// Returns the number of entries in the dictionary.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns `true` if the dictionary has no entries.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns a pointer to the object for `key`, or `nullptr` if absent.
    [[nodiscard]] const object* get(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the object for `key`, or `nullptr` if absent.
    [[nodiscard]] object* get(const std::string& key) noexcept;

    /// Returns a pointer to the boolean object for `key`,
    /// or `nullptr` if absent or not a boolean.
    [[nodiscard]] const boolean_object* get_boolean(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the boolean object for `key`,
    /// or `nullptr` if absent or not a boolean.
    [[nodiscard]] boolean_object* get_boolean(const std::string& key) noexcept;

    /// Returns a pointer to the number object for `key`,
    /// or `nullptr` if absent or not a number.
    [[nodiscard]] const number_object* get_number(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the number object for `key`,
    /// or `nullptr` if absent or not a number.
    [[nodiscard]] number_object* get_number(const std::string& key) noexcept;

    /// Returns a pointer to the string object for `key`,
    /// or `nullptr` if absent or not a string.
    [[nodiscard]] const string_object* get_string(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the string object for `key`,
    /// or `nullptr` if absent or not a string.
    [[nodiscard]] string_object* get_string(const std::string& key) noexcept;

    /// Returns a pointer to the name object for `key`,
    /// or `nullptr` if absent or not a name.
    [[nodiscard]] const name_object* get_name(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the name object for `key`,
    /// or `nullptr` if absent or not a name.
    [[nodiscard]] name_object* get_name(const std::string& key) noexcept;

    /// Returns a pointer to the array object for `key`,
    /// or `nullptr` if absent or not an array.
    [[nodiscard]] const array_object* get_array(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the array object for `key`,
    /// or `nullptr` if absent or not an array.
    [[nodiscard]] array_object* get_array(const std::string& key) noexcept;

    /// Returns a pointer to the nested dictionary object for `key`,
    /// or `nullptr` if absent or not a dictionary.
    [[nodiscard]] const dictionary_object* get_dictionary(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the nested dictionary object for `key`,
    /// or `nullptr` if absent or not a dictionary.
    [[nodiscard]] dictionary_object* get_dictionary(const std::string& key) noexcept;

    /// Returns a pointer to the indirect reference object for `key`,
    /// or `nullptr` if absent or not an indirect reference.
    [[nodiscard]] const indirect_reference*
    get_indirect_reference(const std::string& key) const noexcept;

    /// Returns a mutable pointer to the indirect reference object for `key`,
    /// or `nullptr` if absent or not an indirect reference.
    [[nodiscard]] indirect_reference* get_indirect_reference(const std::string& key) noexcept;

    /// Returns the raw underlying map for full traversal or serialization.
    [[nodiscard]] const dictionary_map_type& entries() const noexcept;

    /// Returns a mutable reference to the raw underlying map.
    [[nodiscard]] dictionary_map_type& entries() noexcept;

private:
    dictionary_map_type entries_;
};

/// A PDF stream object. Consists of a content dictionary and a byte stream payload.
///
/// The content dictionary holds metadata about the stream (e.g. `/Length`, `/Filter`).
/// The byte stream holds the actual data payload, which may be preloaded in memory
/// or deferred for later loading.
class stream_object
{
public:
    /// Construct a stream with the given content dictionary and byte stream.
    explicit stream_object(class dictionary_object dict, class stream stream) noexcept;

    /// Returns a const reference to the content dictionary of this stream.
    [[nodiscard]] const class dictionary_object& dictionary() const noexcept;

    /// Returns a mutable reference to the content dictionary of this stream.
    [[nodiscard]] class dictionary_object& dictionary() noexcept;

    /// Returns a const reference to the byte stream payload of this stream.
    [[nodiscard]] const class stream& stream() const noexcept;

    /// Returns a mutable reference to the byte stream payload of this stream.
    [[nodiscard]] class stream& stream() noexcept;

    /// Write bytes to the end of the stream.
    ///
    /// Proxy method to `stream::write`. Automatically syncs the length of the stream
    /// in the content dictionary with the actual stream size after writing.
    void write(std::span<std::byte> in);

    /// Sets the `Length` value of the object dictionary in the content dictionary to the given
    /// value.
    void set_length(std::size_t length);

    /// Synchronizes the `Length` value of the object dictionary in the content dictionary with the
    /// actual stream size.
    void sync_length();

    /// Returns true if the stream data has been decoded via `content()`.
    [[nodiscard]] bool is_decoded() const noexcept;

    /// Manually set the decoded state flag.
    void set_decoded(bool state) noexcept;

    /// Returns decoded content. If not yet decoded, decodes in-place using the
    /// static filter_manager. After this call, stream().data() returns decoded
    /// bytes and raw bytes are lost.
    /// If no /Filter is present, data is inherently decoded (no-op).
    ///
    /// @param limits bounds the decoded output size; defaults are used when omitted.
    /// @throws parse_exception if /Filter is unknown or the output exceeds `limits`.
    [[nodiscard]] std::span<const std::byte> content(const resource_limits& limits = {});

    /// Returns the current stream data as-is. Does not trigger decode.
    /// Before content(): returns raw bytes.
    /// After content(): returns decoded bytes.
    [[nodiscard]] std::span<const std::byte> raw() const noexcept;

private:
    class dictionary_object dict_;
    class stream stream_;
    bool is_decoded_ = false;
};

/// A discriminated union representing any PDF direct object.
///
/// Covers all primitive and composite PDF object types as defined in the PDF spec:
///   - Null (`null_object`)
///   - Boolean (`boolean_object`)
///   - Number (`number_object`, holds integer or real)
///   - String (`string_object`, with encoding + form provenance)
///   - Name (`name_object`)
///   - Array (`array_object`)
///   - Dictionary (`dictionary_object`, stored via unique_ptr)
///   - Stream (`stream_object`, stored via unique_ptr)
///   - Indirect Reference (`indirect_reference`)
///
/// ## Type safety
///
/// `name_object` and `string_object` are distinct alternatives to prevent
/// accidental mixing of PDF name objects and PDF string objects, which have
/// different syntax and semantics in the spec.
///
/// ## Access patterns
///
/// Type-check helpers (`is_*`) and typed accessors (`as_*`) are provided for
/// ergonomic use in parsing and traversal code. When performance or exhaustive
/// matching is needed, `variant()` exposes the raw `std::variant` for use with
/// `std::visit`.
class object
{
public:
    /// The underlying variant type holding all possible PDF object alternatives.
    using variant_type = std::variant<null_object,                        ///> null
                                      boolean_object,                     ///> boolean
                                      number_object,                      ///> number
                                      string_object,                      ///> string
                                      name_object,                        ///> name
                                      indirect_reference,                 ///> reference
                                      array_object,                       ///> array
                                      std::unique_ptr<dictionary_object>, ///> dictionary
                                      std::unique_ptr<stream_object>      ///> stream
                                      >;

    /// Construct a null PDF object.
    object() noexcept;

    /// Construct a boolean PDF object.
    explicit object(bool value) noexcept;

    /// Construct a boolean PDF object.
    explicit object(boolean_object value) noexcept;

    /// Construct an integer PDF object.
    explicit object(std::int64_t value) noexcept;

    /// Construct a real PDF object.
    /// @throws parse_exception if value is NaN or infinite.
    explicit object(double value);

    /// Construct a number PDF object.
    explicit object(number_object value) noexcept;

    /// Construct a string PDF object.
    ///
    /// May throw `std::bad_alloc`; allocation failures are intentionally not
    /// converted into termination.
    explicit object(std::string value);

    /// Construct a string PDF object.
    explicit object(string_object value) noexcept;

    /// Construct a name PDF object.
    explicit object(name_object value) noexcept;

    /// Construct an array PDF object.
    explicit object(array_object value) noexcept;

    /// Construct a dictionary PDF object.
    explicit object(dictionary_object value);

    /// Construct a stream PDF object.
    explicit object(stream_object value);

    /// Construct an indirect reference PDF object.
    explicit object(indirect_reference value) noexcept;

    ~object();

    object(const object& other);
    object(object&&) noexcept;

    object& operator=(const object& other);
    object& operator=(object&&) noexcept;

    /// Create a deep copy of this object.
    ///
    /// Dictionaries and streams are fully cloned recursively; primitive types,
    /// names, strings, arrays, and indirect references are copied by value.
    [[nodiscard]] object clone() const;

    /// Returns `true` if this object holds a PDF null object.
    [[nodiscard]] bool is_null() const noexcept;

    /// Returns `true` if this object holds a PDF boolean object.
    [[nodiscard]] bool is_boolean() const noexcept;

    /// Returns `true` if this object holds a PDF number object.
    [[nodiscard]] bool is_number() const noexcept;

    /// Returns `true` if this object holds a PDF integer object.
    [[nodiscard]] bool is_integer() const noexcept;

    /// Returns `true` if this object holds a PDF real object.
    [[nodiscard]] bool is_real() const noexcept;

    /// Returns `true` if this object holds a PDF string object.
    [[nodiscard]] bool is_string() const noexcept;

    /// Returns `true` if this object holds a PDF name object.
    [[nodiscard]] bool is_name() const noexcept;

    /// Returns `true` if this object holds a PDF array object.
    [[nodiscard]] bool is_array() const noexcept;

    /// Returns `true` if this object holds a PDF dictionary object.
    [[nodiscard]] bool is_dictionary() const noexcept;

    /// Returns `true` if this object holds a PDF stream object.
    [[nodiscard]] bool is_stream() const noexcept;

    /// Returns `true` if this object holds a PDF indirect reference.
    [[nodiscard]] bool is_indirect_reference() const noexcept;

    /// Returns a pointer to the held boolean, or `nullptr` if this is not a boolean object.
    [[nodiscard]] const boolean_object* as_boolean() const noexcept;

    /// Returns a mutable pointer to the held boolean, or `nullptr` if this is not a boolean object.
    [[nodiscard]] boolean_object* as_boolean() noexcept;

    /// Returns a pointer to the held number, or `nullptr` if this is not a number object.
    [[nodiscard]] const number_object* as_number() const noexcept;

    /// Returns a mutable pointer to the held number, or `nullptr` if this is not a number object.
    [[nodiscard]] number_object* as_number() noexcept;

    /// Returns a pointer to the held string, or `nullptr` if this is not a string object.
    [[nodiscard]] const string_object* as_string() const noexcept;

    /// Returns a mutable pointer to the held string, or `nullptr` if this is not a string object.
    [[nodiscard]] string_object* as_string() noexcept;

    /// Returns a pointer to the held name, or `nullptr` if this is not a name object.
    [[nodiscard]] const name_object* as_name() const noexcept;

    /// Returns a mutable pointer to the held name, or `nullptr` if this is not a name object.
    [[nodiscard]] name_object* as_name() noexcept;

    /// Returns a pointer to the held array, or `nullptr` if this is not an array object.
    [[nodiscard]] const array_object* as_array() const noexcept;

    /// Returns a mutable pointer to the held array, or `nullptr` if this is not an array object.
    [[nodiscard]] array_object* as_array() noexcept;

    /// Returns a pointer to the held dictionary, or `nullptr` if this is not a dictionary object.
    [[nodiscard]] const dictionary_object* as_dictionary() const noexcept;

    /// Returns a pointer to the held dictionary, or `nullptr` if this is not a dictionary object.
    [[nodiscard]] dictionary_object* as_dictionary() noexcept;

    /// Returns a pointer to the held stream, or `nullptr` if this is not a stream object.
    [[nodiscard]] const stream_object* as_stream() const noexcept;

    /// Returns a pointer to the held stream, or `nullptr` if this is not a stream object.
    [[nodiscard]] stream_object* as_stream() noexcept;

    /// Returns a pointer to the held indirect reference, or `nullptr` if this is not an indirect
    /// reference object.
    [[nodiscard]] const indirect_reference* as_indirect_reference() const noexcept;

    /// Returns a mutable pointer to the held indirect reference, or `nullptr` if this is not an
    /// indirect reference object.
    [[nodiscard]] indirect_reference* as_indirect_reference() noexcept;

    /// Returns the raw underlying variant for use with `std::visit`.
    ///
    /// Prefer the typed `as_*` accessors for single-type access.
    /// Use this when exhaustive matching over all alternatives is needed.
    [[nodiscard]] const variant_type& variant() const noexcept;

    /// Returns a mutable reference to the raw underlying variant.
    [[nodiscard]] variant_type& variant() noexcept;

private:
    variant_type value_;
};

} // namespace ripper::pdf::core
