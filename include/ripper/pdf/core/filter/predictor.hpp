#pragma once

#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/util/resource_limits.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace ripper::pdf::core
{
/// Returns `true` when `params` declares a `Predictor` entry greater than 1.
///
/// Used by the filter pipeline to decide whether decode output must be
/// post-processed with a predictor.
[[nodiscard]] bool has_predictor(const dictionary_object* params);

/// Reverses the effect of a stream predictor described by `/DecodeParms`.
///
/// Supported values (PDF 32000-1 §7.4.4):
///   - 1        no predictor (identity)
///   - 2        TIFF predictor 2 (horizontal differencing, 8-bit samples)
///   - 10..15   PNG predictor family (per-row filter-type byte)
///
/// The `/Columns`, `/Colors`, and `/BitsPerComponent` parameters control
/// row layout. Rows of a PNG-predicted stream are each prefixed by a single
/// filter-type byte selecting how that row was encoded.
///
/// @throws parse_exception if the predictor value is unsupported, parameters
///         are invalid, or the input is truncated.
[[nodiscard]] std::vector<std::byte> apply_predictor(std::span<const std::byte> data,
                                                     const dictionary_object& params,
                                                     const resource_limits& limits = {});
} // namespace ripper::pdf::core
