#pragma once

// Length unit resolution: lexbor's parse-time <length> (number + unit) into a
// computed LengthValue. Percentages are NOT resolved here — they pass through
// as LengthValue::Percent for layout to resolve against a containing block.
//
// Milestone scope:
//   M1 (now): px, em, rem, and absolute units (cm/mm/in/pt/pc/Q) fully; ex/ch/
//             viewport units use documented approximations.
//   M2:       ex/ch/cap/ic/lh/rlh + vw/vh/vi/vb/vmin/vmax via QFontMetricsF and
//             a real viewport (see font_metrics.h). Hook points are marked.

#include "value_types.h"

// lexbor C value structs.
#include "lexbor/css/value.h"

namespace style {

// Everything the unit resolver needs from the element's environment.
struct ResolveContext {
  float fontSizePx = 16.0f;       // this element's computed font-size (for em)
  float parentFontSizePx = 16.0f; // parent's font-size (for em on font-size)
  float rootFontSizePx = 16.0f;   // root element font-size (for rem)
  float lineHeightPx = 19.2f;     // computed line-height in px (for lh)
  float rootLineHeightPx = 19.2f; // root line-height (for rlh)
  float viewportW = 0.0f;         // viewport width px (vw/vi/vmin/vmax)
  float viewportH = 0.0f;         // viewport height px (vh/vb/vmin/vmax)

  // M2: QFontMetricsF-derived metrics for the element's font.
  float exPx = 8.0f; // x-height (em*0.5 approx until M2)
  float chPx = 8.0f; // advance of '0' (em*0.5 approx until M2)
};

// Resolve a bare <length> to absolute pixels. `forFontSize` selects the em base
// (parent font-size vs own) per spec.
float resolveLengthPx(const lxb_css_value_length_t& len, const ResolveContext& ctx,
                      bool forFontSize = false);

// Resolve a <length-percentage> typed value to a computed LengthValue. Keywords
// (auto/none/inherit/...) are handled by the resolver before this is called;
// this sees only concrete length / percentage payloads.
LengthValue resolveLengthPercentage(const lxb_css_value_length_percentage_t& lp,
                                    const ResolveContext& ctx);

} // namespace style
