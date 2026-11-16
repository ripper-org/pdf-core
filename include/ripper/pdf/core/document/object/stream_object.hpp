#pragma once

/// The stream_object class definition lives in object.hpp alongside
/// object, due to a mutual-recursion cycle (stream_object holds a
/// dictionary_object, whose unordered_map<object> needs object complete,
/// while object's variant holds unique_ptr<stream_object>).
///
/// This header exists for API symmetry. Every PDF object type has a
/// *_object.hpp entry point.

#include "ripper/pdf/core/document/object/object.hpp"

// Design note (stored bytes are authoritative):
//
// A stream keeps exactly the bytes as stored (`stream()` / `raw()`). `decode()`
// returns a transient `decoded_stream` value and never mutates the object:
// unfiltered streams borrow `raw()`, filtered streams own a fresh buffer.
//
// Consequence: decoding costs one inflate per call, but a stream that is only
// inspected is written back verbatim, even for codecs with no in-tree encoder
// (DCTDecode, JBIG2Decode, JPXDecode). Re-encoding only happens when a caller
// explicitly replaces the stored bytes (for example with
// `filter_manager::encode`).
