#pragma once

/// The dictionary_object class definition lives in object.hpp alongside
/// object, due to a mutual-recursion cycle (dictionary_object needs object
/// complete for its unordered_map, while object's variant holds
/// unique_ptr<dictionary_object>).
///
/// This header exists for API symmetry. Every PDF object type has a
/// *_object.hpp entry point.

#include "ripper/pdf/core/document/object/object.hpp"
