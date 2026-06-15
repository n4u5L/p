#pragma once

// Per-property metadata table, keyed by lexbor's property id
// (LXB_CSS_PROPERTY_*). This encodes the facts lexbor does NOT carry:
//   - inheritance (lxb_css_entry_data_t has no `inherited` flag — confirmed),
//   - which COW group a property's computed value lives in,
//   - the resolution phase (intra-element dependency order: writing-mode →
//     color → font-size → font props → line-height → everything else).
//
// The table is indexed directly by property id, so lookups are O(1).

#include <cstddef>
#include <cstdint>

namespace style {

// COW storage group. Mirrors the grouping in computed_style.h. Group is about
// write-time co-variance / sharing, and is independent of `inherited`.
enum class Group : uint8_t {
  InheritedText,  // color, font, text-* — inherited, changes together
  InheritedOther, // direction, writing-mode, visibility
  Box,            // display, position, sizing, flex
  Surround,       // margin/padding/border/inset
  Visual,         // background, opacity, overflow, decoration
  Shorthand       // shorthands (margin, border, flex, font...) — expanded, not stored
};

// Intra-element resolution phase. The resolver runs properties in ascending
// phase so that context (writing-mode, color, font-size, metrics, line-height)
// is ready before dependents consume it.
enum class Phase : uint8_t {
  WritingContext = 0, // writing-mode, direction  (logical→physical mapping)
  ColorContext = 1,   // color                    (currentColor source)
  FontSize = 2,       // font-size                (em/ex/ch base)
  FontProps = 3,      // font-family/style/weight (QFont → metrics)
  LineHeight = 4,     // line-height              (lh unit)
  Normal = 5          // everything else
};

struct PropertyMeta {
  uint16_t id = 0;        // lexbor LXB_CSS_PROPERTY_*
  bool inherited = false; // CSS-spec inheritance
  Group group = Group::Box;
  Phase phase = Phase::Normal;
  bool isShorthand = false; // expanded into longhands by lexbor; skipped by resolver
};

// Number of table slots: LXB_CSS_PROPERTY__LAST_ENTRY (0x67). Defined in .cpp
// to avoid leaking the lexbor header here.
size_t propertyCount();

// O(1) metadata lookup. `id` must be < propertyCount(); out-of-range yields a
// default (non-inherited, Box, Normal) meta.
const PropertyMeta& propertyMeta(uint16_t id);

inline bool isInherited(uint16_t id) {
  return propertyMeta(id).inherited;
}

// Context-provider predicates used by the resolver's phased loop. Defined in
// .cpp against the lexbor enum values.
bool isColorProperty(uint16_t id);  // value may contain currentColor
bool isLengthProperty(uint16_t id); // value is <length-percentage>-ish

} // namespace style
