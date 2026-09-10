#pragma once

/// The stream_object class definition lives in object.hpp alongside
/// object, due to a mutual-recursion cycle (stream_object holds a
/// dictionary_object, whose unordered_map<object> needs object complete,
/// while object's variant holds unique_ptr<stream_object>).
///
/// This header exists for API symmetry. Every PDF object type has a
/// *_object.hpp entry point.

#include "ripper/pdf/core/document/object/object.hpp"

// Design note (single-buffer stream storage, intentionally NOT the dual-buffer
// layout of roadmap item 1.3):
//
// A stream keeps exactly one byte buffer. `content()` decodes the stream in
// place — after the first call the buffer holds the decoded bytes and the
// original on-disk encoded bytes are gone (`raw()` then returns the decoded
// data). This keeps the common read-and-inspect path zero-copy and simple.
//
// Consequence: an object that is decoded and then re-saved is re-encoded from
// the in-memory decoded bytes (see `filter_manager::encode`). For lossy codecs
// whose encoder is lossless-by-design this is fine; for codecs with no in-tree
// encoder (DCTDecode, JBIG2Decode, JPXDecode) a save after decoding is not
// currently supported and throws. Callers that must preserve the exact on-disk
// stream bytes should avoid calling `content()` on streams they intend to save
// verbatim.
