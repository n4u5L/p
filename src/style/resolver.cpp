#include "resolver.h"

#include "property_meta.h"

#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/css/property.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/css/property/const.h"
#include "lexbor/css/value.h"
#include "lexbor/css/value/const.h"
#include "lexbor/css/rule.h"
#include "lexbor/style/dom/interfaces/element.h"

namespace style {

namespace {

enum class Wide { None,
                  Inherit,
                  Initial,
                  Unset,
                  Revert };

Wide wideFromType(lxb_css_value_type_t t) {
  switch (t) {
  case LXB_CSS_VALUE_INHERIT:
    return Wide::Inherit;
  case LXB_CSS_VALUE_INITIAL:
    return Wide::Initial;
  case LXB_CSS_VALUE_UNSET:
    return Wide::Unset;
  case LXB_CSS_VALUE_REVERT:
    return Wide::Revert;
  default:
    return Wide::None;
  }
}

bool isUnsetSide(const lxb_css_value_length_percentage_t& lp) {
  return lp.type == LXB_CSS_VALUE__UNDEF;
}

// Fetch the typed value pointer for canonical property `id`, or nullptr if no
// declaration won the cascade for it. All union members alias `u.user`.
template <class T>
const T* declared(const CascadedStyle& cascaded, uint16_t id) {
  return cascaded.value<T>(id);
}

bool convColor(const lxb_css_value_color_t& c, const Color& currentColorValue,
               Color& out);

const lxb_css_value_length_percentage_t* declaredLength(
    const CascadedStyle& cascaded, uint16_t id) {
  const CascadedDeclaration* d = cascaded.declaration(id);
  if (d == nullptr || d->declaration == nullptr) {
    return nullptr;
  }

  switch (d->sourceProperty) {
  case LXB_CSS_PROPERTY_MARGIN:
    if (const auto* margin =
            static_cast<const lxb_css_property_margin_t*>(
                d->declaration->u.user)) {
      switch (id) {
      case LXB_CSS_PROPERTY_MARGIN_TOP:
        return &margin->top;
      case LXB_CSS_PROPERTY_MARGIN_RIGHT:
        return isUnsetSide(margin->right) ? &margin->top : &margin->right;
      case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
        return isUnsetSide(margin->bottom) ? &margin->top : &margin->bottom;
      case LXB_CSS_PROPERTY_MARGIN_LEFT:
        if (!isUnsetSide(margin->left)) return &margin->left;
        return isUnsetSide(margin->right) ? &margin->top : &margin->right;
      default:
        break;
      }
    }
    return nullptr;

  case LXB_CSS_PROPERTY_PADDING:
    if (const auto* padding =
            static_cast<const lxb_css_property_padding_t*>(
                d->declaration->u.user)) {
      switch (id) {
      case LXB_CSS_PROPERTY_PADDING_TOP:
        return &padding->top;
      case LXB_CSS_PROPERTY_PADDING_RIGHT:
        return isUnsetSide(padding->right) ? &padding->top : &padding->right;
      case LXB_CSS_PROPERTY_PADDING_BOTTOM:
        return isUnsetSide(padding->bottom) ? &padding->top : &padding->bottom;
      case LXB_CSS_PROPERTY_PADDING_LEFT:
        if (!isUnsetSide(padding->left)) return &padding->left;
        return isUnsetSide(padding->right) ? &padding->top : &padding->right;
      default:
        break;
      }
    }
    return nullptr;

  default:
    return static_cast<const lxb_css_value_length_percentage_t*>(
        d->declaration->u.user);
  }
}

// --- enum mappers (lexbor value id → our compact enum) ---------------------

Display mapDisplay(lxb_css_display_type_t a) {
  switch (a) {
  case LXB_CSS_DISPLAY_BLOCK:
    return Display::Block;
  case LXB_CSS_DISPLAY_INLINE:
    return Display::Inline;
  case LXB_CSS_DISPLAY_INLINE_BLOCK:
    return Display::InlineBlock;
  case LXB_CSS_DISPLAY_FLEX:
    return Display::Flex;
  case LXB_CSS_DISPLAY_INLINE_FLEX:
    return Display::InlineFlex;
  case LXB_CSS_DISPLAY_LIST_ITEM:
    return Display::ListItem;
  case LXB_CSS_DISPLAY_TABLE:
    return Display::Table;
  case LXB_CSS_DISPLAY_TABLE_ROW:
    return Display::TableRow;
  case LXB_CSS_DISPLAY_TABLE_CELL:
    return Display::TableCell;
  case LXB_CSS_DISPLAY_NONE:
    return Display::None;
  default:
    return Display::Inline;
  }
}

Position mapPosition(lxb_css_position_type_t t) {
  switch (t) {
  case LXB_CSS_POSITION_RELATIVE:
    return Position::Relative;
  case LXB_CSS_POSITION_ABSOLUTE:
    return Position::Absolute;
  case LXB_CSS_POSITION_FIXED:
    return Position::Fixed;
  case LXB_CSS_POSITION_STICKY:
    return Position::Sticky;
  default:
    return Position::Static;
  }
}

Visibility mapVisibility(lxb_css_visibility_type_t t) {
  switch (t) {
  case LXB_CSS_VISIBILITY_HIDDEN:
    return Visibility::Hidden;
  case LXB_CSS_VISIBILITY_COLLAPSE:
    return Visibility::Collapse;
  default:
    return Visibility::Visible;
  }
}

BoxSizing mapBoxSizing(lxb_css_box_sizing_type_t t) {
  return t == LXB_CSS_BOX_SIZING_BORDER_BOX ? BoxSizing::BorderBox
                                            : BoxSizing::ContentBox;
}

Float mapFloat(lxb_css_float_type_t t) {
  switch (t) {
  case LXB_CSS_FLOAT_LEFT:
    return Float::Left;
  case LXB_CSS_FLOAT_RIGHT:
    return Float::Right;
  default:
    return Float::None;
  }
}

Clear mapClear(lxb_css_clear_type_t t) {
  switch (t) {
  case LXB_CSS_CLEAR_LEFT:
  case LXB_CSS_CLEAR_INLINE_START:
  case LXB_CSS_CLEAR_BLOCK_START:
    return Clear::Left;
  case LXB_CSS_CLEAR_RIGHT:
  case LXB_CSS_CLEAR_INLINE_END:
  case LXB_CSS_CLEAR_BLOCK_END:
    return Clear::Right;
  case LXB_CSS_CLEAR_TOP:
  case LXB_CSS_CLEAR_BOTTOM:
    return Clear::Both;
  default:
    return Clear::None;
  }
}

Direction mapDirection(lxb_css_direction_type_t t) {
  return t == LXB_CSS_DIRECTION_RTL ? Direction::Rtl : Direction::Ltr;
}

WritingMode mapWritingMode(lxb_css_writing_mode_type_t t) {
  switch (t) {
  case LXB_CSS_WRITING_MODE_VERTICAL_RL:
    return WritingMode::VerticalRl;
  case LXB_CSS_WRITING_MODE_VERTICAL_LR:
    return WritingMode::VerticalLr;
  default:
    return WritingMode::HorizontalTb;
  }
}

Overflow mapOverflow(lxb_css_overflow_x_type_t t) {
  switch (t) {
  case LXB_CSS_OVERFLOW_X_HIDDEN:
    return Overflow::Hidden;
  case LXB_CSS_OVERFLOW_X_SCROLL:
    return Overflow::Scroll;
  case LXB_CSS_OVERFLOW_X_AUTO:
    return Overflow::Auto;
  case LXB_CSS_OVERFLOW_X_CLIP:
    return Overflow::Clip;
  default:
    return Overflow::Visible;
  }
}

TextAlign mapTextAlign(lxb_css_text_align_type_t t) {
  switch (t) {
  case LXB_CSS_TEXT_ALIGN_END:
    return TextAlign::End;
  case LXB_CSS_TEXT_ALIGN_LEFT:
    return TextAlign::Left;
  case LXB_CSS_TEXT_ALIGN_RIGHT:
    return TextAlign::Right;
  case LXB_CSS_TEXT_ALIGN_CENTER:
    return TextAlign::Center;
  case LXB_CSS_TEXT_ALIGN_JUSTIFY:
  case LXB_CSS_TEXT_ALIGN_JUSTIFY_ALL:
    return TextAlign::Justify;
  case LXB_CSS_TEXT_ALIGN_MATCH_PARENT:
  case LXB_CSS_TEXT_ALIGN_START:
  default:
    return TextAlign::Start;
  }
}

WhiteSpace mapWhiteSpace(lxb_css_white_space_type_t t) {
  switch (t) {
  case LXB_CSS_WHITE_SPACE_PRE:
    return WhiteSpace::Pre;
  case LXB_CSS_WHITE_SPACE_NOWRAP:
    return WhiteSpace::Nowrap;
  case LXB_CSS_WHITE_SPACE_PRE_WRAP:
  case LXB_CSS_WHITE_SPACE_BREAK_SPACES:
    return WhiteSpace::PreWrap;
  case LXB_CSS_WHITE_SPACE_PRE_LINE:
    return WhiteSpace::PreLine;
  default:
    return WhiteSpace::Normal;
  }
}

FontStyle mapFontStyle(lxb_css_font_style_type_t t) {
  switch (t) {
  case LXB_CSS_FONT_STYLE_ITALIC:
    return FontStyle::Italic;
  case LXB_CSS_FONT_STYLE_OBLIQUE:
    return FontStyle::Oblique;
  default:
    return FontStyle::Normal;
  }
}

float fontStretchPercent(lxb_css_font_stretch_type_t t) {
  switch (t) {
  case LXB_CSS_FONT_STRETCH_ULTRA_CONDENSED:
    return 50.0f;
  case LXB_CSS_FONT_STRETCH_EXTRA_CONDENSED:
    return 62.5f;
  case LXB_CSS_FONT_STRETCH_CONDENSED:
    return 75.0f;
  case LXB_CSS_FONT_STRETCH_SEMI_CONDENSED:
    return 87.5f;
  case LXB_CSS_FONT_STRETCH_SEMI_EXPANDED:
    return 112.5f;
  case LXB_CSS_FONT_STRETCH_EXPANDED:
    return 125.0f;
  case LXB_CSS_FONT_STRETCH_EXTRA_EXPANDED:
    return 150.0f;
  case LXB_CSS_FONT_STRETCH_ULTRA_EXPANDED:
    return 200.0f;
  default:
    return 100.0f;
  }
}

const char* genericFontFamily(lxb_css_font_family_type_t t) {
  switch (t) {
  case LXB_CSS_FONT_FAMILY_SERIF:
  case LXB_CSS_FONT_FAMILY_UI_SERIF:
    return "serif";
  case LXB_CSS_FONT_FAMILY_MONOSPACE:
  case LXB_CSS_FONT_FAMILY_UI_MONOSPACE:
    return "monospace";
  case LXB_CSS_FONT_FAMILY_CURSIVE:
    return "cursive";
  case LXB_CSS_FONT_FAMILY_FANTASY:
    return "fantasy";
  case LXB_CSS_FONT_FAMILY_SYSTEM_UI:
    return "system-ui";
  case LXB_CSS_FONT_FAMILY_EMOJI:
    return "emoji";
  case LXB_CSS_FONT_FAMILY_MATH:
    return "math";
  case LXB_CSS_FONT_FAMILY_FANGSONG:
    return "fangsong";
  case LXB_CSS_FONT_FAMILY_UI_ROUNDED:
    return "ui-rounded";
  case LXB_CSS_FONT_FAMILY_SANS_SERIF:
  case LXB_CSS_FONT_FAMILY_UI_SANS_SERIF:
  default:
    return "sans-serif";
  }
}

BorderStyle mapBorderStyle(lxb_css_value_type_t t) {
  switch (t) {
  case LXB_CSS_BORDER_HIDDEN:
    return BorderStyle::Hidden;
  case LXB_CSS_BORDER_DOTTED:
    return BorderStyle::Dotted;
  case LXB_CSS_BORDER_DASHED:
    return BorderStyle::Dashed;
  case LXB_CSS_BORDER_SOLID:
    return BorderStyle::Solid;
  case LXB_CSS_BORDER_DOUBLE:
    return BorderStyle::Double;
  case LXB_CSS_BORDER_GROOVE:
    return BorderStyle::Groove;
  case LXB_CSS_BORDER_RIDGE:
    return BorderStyle::Ridge;
  case LXB_CSS_BORDER_INSET:
    return BorderStyle::Inset;
  case LXB_CSS_BORDER_OUTSET:
    return BorderStyle::Outset;
  default:
    return BorderStyle::None;
  }
}

LengthValue borderWidth(lxb_css_value_length_type_t width,
                        const ResolveContext& ctx) {
  switch (width.type) {
  case LXB_CSS_BORDER_THIN:
    return LengthValue::makePx(1.0f);
  case LXB_CSS_BORDER_THICK:
    return LengthValue::makePx(5.0f);
  case LXB_CSS_BORDER__LENGTH:
    return LengthValue::makePx(resolveLengthPx(width.length, ctx));
  case LXB_CSS_BORDER_MEDIUM:
  case LXB_CSS_VALUE__UNDEF:
  default:
    return LengthValue::makePx(3.0f);
  }
}

BorderEdge resolveBorderEdgeValue(const lxb_css_property_border_t& b,
                                  const ResolveContext& ctx,
                                  const Color& currentColor) {
  BorderEdge edge;
  edge.styleKind = mapBorderStyle(b.style);
  edge.width = (edge.styleKind == BorderStyle::None ||
                edge.styleKind == BorderStyle::Hidden)
                   ? LengthValue::makePx(0.0f)
                   : borderWidth(b.width, ctx);

  Color out;
  if (b.color.type == LXB_CSS_VALUE__UNDEF) {
    edge.color = currentColor;
  } else if (convColor(b.color, currentColor, out)) {
    edge.color = out;
  }
  return edge;
}

BorderEdge resolveBorderEdge(const CascadedStyle& cascaded, uint16_t id,
                             const ResolveContext& ctx,
                             const Color& currentColor,
                             const BorderEdge& parentEdge,
                             bool* present) {
  const auto* b = declared<lxb_css_property_border_t>(cascaded, id);
  *present = b != nullptr;
  if (b == nullptr) {
    return BorderEdge{};
  }

  switch (wideFromType(b->style)) {
  case Wide::Inherit:
    return parentEdge;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    return BorderEdge{};
  case Wide::None:
    return resolveBorderEdgeValue(*b, ctx, currentColor);
  }
  return BorderEdge{};
}

void resolveBorderColor(const CascadedStyle& cascaded, uint16_t id,
                        const Color& currentColor, const BorderEdge& parentEdge,
                        BorderEdge& edge) {
  const auto* c = declared<lxb_css_value_color_t>(cascaded, id);
  if (c == nullptr) {
    return;
  }

  Color out;
  switch (wideFromType(c->type)) {
  case Wide::Inherit:
    edge.color = parentEdge.color;
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    edge.color = currentColor;
    break;
  case Wide::None:
    if (convColor(*c, currentColor, out)) edge.color = out;
    break;
  }
}

// Convert a lexbor number-or-percentage channel to 0..255.
uint8_t channel8(const lxb_css_value_number_percentage_t& c) {
  if (c.type == LXB_CSS_VALUE__PERCENTAGE) {
    float v = static_cast<float>(c.u.percentage.num) * 2.55f;
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
  }
  float v = static_cast<float>(c.u.number.num);
  return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

uint8_t alpha8(const lxb_css_value_number_percentage_t& a) {
  float v;
  if (a.type == LXB_CSS_VALUE__PERCENTAGE) {
    v = static_cast<float>(a.u.percentage.num) * 2.55f;
  } else {
    v = static_cast<float>(a.u.number.num) * 255.0f;
  }
  return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// Resolve a lexbor color into a concrete Color. `currentColorValue` is the
// element's already-computed `color` (for currentColor on non-color props).
// Returns false when the value is a wide keyword (handled by the caller).
bool convColor(const lxb_css_value_color_t& c, const Color& currentColorValue,
               Color& out) {
  switch (c.type) {
  case LXB_CSS_VALUE_HEX:
    out = Color(c.u.hex.rgba.r, c.u.hex.rgba.g, c.u.hex.rgba.b, c.u.hex.rgba.a);
    return true;
  case LXB_CSS_VALUE_RGB:
    out = Color(channel8(c.u.rgb.r), channel8(c.u.rgb.g), channel8(c.u.rgb.b), 255);
    return true;
  case LXB_CSS_VALUE_RGBA:
    out = Color(channel8(c.u.rgb.r), channel8(c.u.rgb.g), channel8(c.u.rgb.b), alpha8(c.u.rgb.a));
    return true;
  case LXB_CSS_VALUE_CURRENTCOLOR:
    out = currentColorValue;
    return true;
  case LXB_CSS_VALUE_TRANSPARENT:
    out = Color::transparent();
    return true;
  case LXB_CSS_VALUE_INHERIT:
  case LXB_CSS_VALUE_INITIAL:
  case LXB_CSS_VALUE_UNSET:
  case LXB_CSS_VALUE_REVERT:
    return false; // wide keyword
  default:
    // Named colors (red/green/...) and hsl/lab/lch: M2 color table.
    out = Color::black();
    return true;
  }
}

// font-size keyword (medium/large/...) → px, against a 16px medium baseline.
float fontSizeKeywordPx(lxb_css_value_type_t t) {
  switch (t) {
  case LXB_CSS_FONT_SIZE_XX_SMALL:
    return 16.0f * 3.0f / 5.0f;
  case LXB_CSS_FONT_SIZE_X_SMALL:
    return 16.0f * 3.0f / 4.0f;
  case LXB_CSS_FONT_SIZE_SMALL:
    return 16.0f * 8.0f / 9.0f;
  case LXB_CSS_FONT_SIZE_MEDIUM:
    return 16.0f;
  case LXB_CSS_FONT_SIZE_LARGE:
    return 16.0f * 6.0f / 5.0f;
  case LXB_CSS_FONT_SIZE_X_LARGE:
    return 16.0f * 3.0f / 2.0f;
  case LXB_CSS_FONT_SIZE_XX_LARGE:
    return 16.0f * 2.0f;
  case LXB_CSS_FONT_SIZE_XXX_LARGE:
    return 16.0f * 3.0f;
  default:
    return 16.0f;
  }
}

// Resolve font-size to px. percentage/em are relative to the parent font-size.
float resolveFontSizePx(const CascadedStyle& cascaded, float parentFontSizePx,
                        const ResolveContext& ctx) {
  using FS = lxb_css_property_font_size_t; // length_percentage_type
  const FS* fs = declared<FS>(cascaded, LXB_CSS_PROPERTY_FONT_SIZE);
  if (!fs) {
    return parentFontSizePx; // inherited
  }
  switch (wideFromType(fs->type)) {
  case Wide::Inherit:
  case Wide::Unset: // font-size is inherited
    return parentFontSizePx;
  case Wide::Initial:
    return 16.0f;
  case Wide::Revert:
    return parentFontSizePx; // M1: revert≈unset
  case Wide::None:
    break;
  }
  // Absolute/relative size keywords sit in fs->type too.
  if (fs->type != LXB_CSS_VALUE__LENGTH && fs->type != LXB_CSS_VALUE__PERCENTAGE) {
    if (fs->type == LXB_CSS_FONT_SIZE_LARGER) return parentFontSizePx * 1.2f;
    if (fs->type == LXB_CSS_FONT_SIZE_SMALLER) return parentFontSizePx / 1.2f;
    return fontSizeKeywordPx(fs->type);
  }
  const lxb_css_value_length_percentage_t& lp = fs->length;
  if (lp.type == LXB_CSS_VALUE__PERCENTAGE) {
    return parentFontSizePx * static_cast<float>(lp.u.percentage.num) / 100.0f;
  }
  // length: em/ex are relative to the PARENT font-size. Build a context whose
  // font base is the parent's size (ctx coming in is the parent's context, so
  // its `fontSizePx` already is the parent's, but parentFontSizePx is the
  // grandparent's — pin both to parentFontSizePx for correctness).
  ResolveContext c = ctx;
  c.fontSizePx = parentFontSizePx;
  c.parentFontSizePx = parentFontSizePx;
  c.exPx = parentFontSizePx * 0.5f;
  c.chPx = parentFontSizePx * 0.5f;
  return resolveLengthPx(lp.u.length, c, /*forFontSize=*/true);
}

// Resolve a <length-percentage> property with wide-keyword + inheritance.
// `initialVal` is the property's initial value; `parentVal` the parent's
// computed value (used only for `inherit`). Non-inherited props pass their
// initial for both initial and unset.
LengthValue resolveLP(const CascadedStyle& cascaded, uint16_t id,
                      const ResolveContext& ctx, LengthValue initialVal,
                      LengthValue parentVal, bool inheritedProp,
                      bool* present) {
  const auto* lp = declaredLength(cascaded, id);
  *present = (lp != nullptr);
  if (!lp) {
    return inheritedProp ? parentVal : initialVal;
  }
  switch (wideFromType(lp->type)) {
  case Wide::Inherit:
    return parentVal;
  case Wide::Initial:
    return initialVal;
  case Wide::Unset:
    return inheritedProp ? parentVal : initialVal;
  case Wide::Revert:
    return inheritedProp ? parentVal : initialVal; // M1
  case Wide::None:
    return resolveLengthPercentage(*lp, ctx);
  }
  return initialVal;
}

LengthValue resolveLPValue(const lxb_css_value_length_percentage_t& lp,
                           const ResolveContext& ctx, LengthValue initialVal,
                           LengthValue parentVal, bool inheritedProp) {
  switch (wideFromType(lp.type)) {
  case Wide::Inherit:
    return parentVal;
  case Wide::Initial:
    return initialVal;
  case Wide::Unset:
    return inheritedProp ? parentVal : initialVal;
  case Wide::Revert:
    return inheritedProp ? parentVal : initialVal; // M1
  case Wide::None:
    return resolveLengthPercentage(lp, ctx);
  }
  return initialVal;
}

void expandFourSides(const lxb_css_value_length_percentage_t* input[4],
                     const lxb_css_value_length_percentage_t* output[4]) {
  unsigned count = 4;
  if (isUnsetSide(*input[1])) {
    count = 1;
  } else if (isUnsetSide(*input[2])) {
    count = 2;
  } else if (isUnsetSide(*input[3])) {
    count = 3;
  }

  output[kTop] = input[kTop];
  output[kRight] = count >= 2 ? input[kRight] : input[kTop];
  output[kBottom] = count >= 3 ? input[kBottom] : input[kTop];
  output[kLeft] = count >= 4 ? input[kLeft] : output[kRight];
}

} // namespace

StyleResolver::StyleResolver() = default;

ComputedStyle*
StyleResolver::resolveElement(lxb_dom_element_t* element,
                              const ComputedStyle& parent,
                              const ResolveContext& parentCtx,
                              ResolveContext& outCtx) {
  ComputedStyle* style = heap_.create<ComputedStyle>();
  style->inheritFrom(parent);
  ComputedStyleBuilder builder(*style, heap_);
  CascadedStyle cascaded = cascadedStyleNormalizer_.collect(element);

  // ---- Phase 0: writing-mode, direction (logical→physical inputs) ---------
  if (const auto* d = declared<lxb_css_property_direction_t>(
          cascaded,
          LXB_CSS_PROPERTY_DIRECTION)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.inheritedOther().direction = Direction::Ltr;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited: keep parent's (already shared)
    case Wide::None:
      builder.inheritedOther().direction = mapDirection(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_writing_mode_t>(
          cascaded,
          LXB_CSS_PROPERTY_WRITING_MODE)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.inheritedOther().writingMode = WritingMode::HorizontalTb;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break;
    case Wide::None:
      builder.inheritedOther().writingMode = mapWritingMode(d->type);
      break;
    }
  }

  // ---- Phase 1: color (currentColor source) -------------------------------
  {
    Color resolved = parent.text().color; // inherited default
    if (const auto* c = declared<lxb_css_property_color_t>(
            cascaded,
            LXB_CSS_PROPERTY_COLOR)) {
      Color out;
      switch (wideFromType(c->type)) {
      case Wide::Initial:
        resolved = Color::black();
        break;
      case Wide::Inherit:
      case Wide::Unset:
      case Wide::Revert:
        resolved = parent.text().color;
        break;
      case Wide::None:
        // For `color`, currentColor means the inherited color.
        if (convColor(*c, parent.text().color, out)) resolved = out;
        break;
      }
    }
    builder.text().color = resolved;
  }
  const Color currentColor = style->text().color;

  // ---- Phase 2: font-size (em/ex/ch base) ---------------------------------
  const float fontSizePx =
      resolveFontSizePx(cascaded, parentCtx.fontSizePx, parentCtx);
  builder.text().font.sizePx = fontSizePx;

  // Build the context used to resolve this element's lengths.
  outCtx = parentCtx;
  outCtx.parentFontSizePx = fontSizePx; // children's em base
  outCtx.fontSizePx = fontSizePx;
  outCtx.exPx = fontSizePx * 0.5f; // M2: QFontMetricsF::xHeight()
  outCtx.chPx = fontSizePx * 0.5f; // M2: advance('0')
  outCtx.lineHeightPx = fontSizePx * 1.2f;
  ResolveContext& ctx = outCtx;

  // ---- Phase 3: font props (font metrics are still M2) --------------------
  if (const auto* ff = declared<lxb_css_property_font_family_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_FAMILY)) {
    if (ff->first != nullptr) {
      const lxb_css_property_family_name_t* name = ff->first;
      if (name->generic) {
        builder.text().font.family = genericFontFamily(name->u.type);
      } else {
        builder.text().font.family.assign(
            reinterpret_cast<const char*>(name->u.str.data),
            name->u.str.length);
      }
    }
  }
  if (const auto* fw = declared<lxb_css_property_font_weight_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_WEIGHT)) {
    switch (wideFromType(fw->type)) {
    case Wide::Initial:
      builder.text().font.weight = 400;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      if (fw->type == LXB_CSS_FONT_WEIGHT_BOLD) {
        builder.text().font.weight = 700;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT_BOLDER) {
        builder.text().font.weight =
            parent.text().font.weight < 600 ? 700 : 900;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT_LIGHTER) {
        builder.text().font.weight =
            parent.text().font.weight >= 600 ? 400 : 100;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT__NUMBER) {
        builder.text().font.weight = static_cast<int>(fw->number.num);
      } else {
        builder.text().font.weight = 400;
      }
      break;
    }
  }
  if (const auto* fs = declared<lxb_css_property_font_style_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_STYLE)) {
    switch (wideFromType(fs->type)) {
    case Wide::Initial:
      builder.text().font.style = FontStyle::Normal;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().font.style = mapFontStyle(fs->type);
      break;
    }
  }
  if (const auto* fst = declared<lxb_css_property_font_stretch_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_STRETCH)) {
    switch (wideFromType(fst->type)) {
    case Wide::Initial:
      builder.text().font.stretchPercent = 100.0f;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      if (fst->type == LXB_CSS_FONT_STRETCH__PERCENTAGE) {
        builder.text().font.stretchPercent =
            static_cast<float>(fst->percentage.num);
      } else {
        builder.text().font.stretchPercent = fontStretchPercent(fst->type);
      }
      break;
    }
  }

  // ---- Phase 4: line-height (lh unit; simplified for M1) ------------------
  if (const auto* lh = declared<lxb_css_property_line_height_t>(
          cascaded,
          LXB_CSS_PROPERTY_LINE_HEIGHT)) {
    LineHeight value = parent.text().lineHeight;
    switch (wideFromType(lh->type)) {
    case Wide::Initial:
      value = LineHeight{};
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      value = parent.text().lineHeight;
      break;
    case Wide::None:
      if (lh->type == LXB_CSS_VALUE__NUMBER) {
        value.kind = LineHeight::Kind::Number;
        value.value = static_cast<float>(lh->u.number.num);
        ctx.lineHeightPx = fontSizePx * value.value;
      } else if (lh->type == LXB_CSS_VALUE__PERCENTAGE) {
        value.kind = LineHeight::Kind::LengthPx;
        value.value = fontSizePx * static_cast<float>(lh->u.percentage.num) / 100.0f;
        ctx.lineHeightPx = value.value;
      } else if (lh->type == LXB_CSS_VALUE__LENGTH) {
        value.kind = LineHeight::Kind::LengthPx;
        value.value = resolveLengthPx(lh->u.length, ctx);
        ctx.lineHeightPx = value.value;
      }
      break;
    }
    builder.text().lineHeight = value;
  }

  // ---- Phase 5: box / sizing enums ----------------------------------------
  if (const auto* d =
          declared<lxb_css_property_display_t>(cascaded, LXB_CSS_PROPERTY_DISPLAY)) {
    switch (wideFromType(d->a)) {
    case Wide::Initial:
    case Wide::Unset:
      builder.box().display = Display::Inline;
      break;
    case Wide::Inherit:
      builder.box().display = parent.box().display;
      break;
    case Wide::Revert:
      builder.box().display = Display::Inline;
      break;
    case Wide::None:
      builder.box().display = mapDisplay(d->a);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_position_t>(
          cascaded,
          LXB_CSS_PROPERTY_POSITION)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.box().position = parent.box().position;
    } else if (wideFromType(d->type) == Wide::None) {
      builder.box().position = mapPosition(d->type);
    } // initial/unset/revert → Static (default already)
  }
  if (const auto* d = declared<lxb_css_property_box_sizing_t>(
          cascaded,
          LXB_CSS_PROPERTY_BOX_SIZING)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.box().boxSizing = parent.box().boxSizing;
    } else if (wideFromType(d->type) == Wide::None) {
      builder.box().boxSizing = mapBoxSizing(d->type);
    }
  }
  if (const auto* d =
          declared<lxb_css_property_float_t>(cascaded, LXB_CSS_PROPERTY_FLOAT)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.box().floatKind = parent.box().floatKind;
    } else if (wideFromType(d->type) == Wide::None) {
      builder.box().floatKind = mapFloat(d->type);
    }
  }
  if (const auto* d =
          declared<lxb_css_property_clear_t>(cascaded, LXB_CSS_PROPERTY_CLEAR)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.box().clear = parent.box().clear;
    } else if (wideFromType(d->type) == Wide::Initial ||
               wideFromType(d->type) == Wide::Unset ||
               wideFromType(d->type) == Wide::Revert) {
      builder.box().clear = Clear::None;
    } else {
      builder.box().clear = mapClear(d->type);
    }
  }
  if (const auto* z =
          declared<lxb_css_property_z_index_t>(cascaded, LXB_CSS_PROPERTY_Z_INDEX)) {
    switch (wideFromType(z->type)) {
    case Wide::Inherit:
      builder.box().zIndex = parent.box().zIndex;
      builder.box().zIndexAuto = parent.box().zIndexAuto;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.box().zIndex = 0;
      builder.box().zIndexAuto = true;
      break;
    case Wide::None:
      if (z->type == LXB_CSS_Z_INDEX__INTEGER) {
        builder.box().zIndex = static_cast<int>(z->integer.num);
        builder.box().zIndexAuto = false;
      } else {
        builder.box().zIndex = 0;
        builder.box().zIndexAuto = true;
      }
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_visibility_t>(
          cascaded,
          LXB_CSS_PROPERTY_VISIBILITY)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.inheritedOther().visibility = Visibility::Visible;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.inheritedOther().visibility = mapVisibility(d->type);
      break;
    }
  }

  // ---- Box sizes (length-percentage; non-inherited) -----------------------
  // For non-inherited props, `inherit` must copy the PARENT's computed value,
  // so each call passes parent's value explicitly.
  bool present = false;
  auto setBoxLP = [&](uint16_t id, LengthValue initial, LengthValue parentVal, LengthValue& slot) {
    LengthValue v = resolveLP(cascaded, id, ctx, initial, parentVal, false, &present);
    if (present) slot = v;
  };
  setBoxLP(LXB_CSS_PROPERTY_WIDTH, LengthValue::makeAuto(), parent.box().width, builder.box().width);
  setBoxLP(LXB_CSS_PROPERTY_HEIGHT, LengthValue::makeAuto(), parent.box().height, builder.box().height);
  setBoxLP(LXB_CSS_PROPERTY_MIN_WIDTH, LengthValue::makePx(0), parent.box().minWidth, builder.box().minWidth);
  setBoxLP(LXB_CSS_PROPERTY_MIN_HEIGHT, LengthValue::makePx(0), parent.box().minHeight, builder.box().minHeight);
  setBoxLP(LXB_CSS_PROPERTY_MAX_WIDTH, LengthValue::makeNone(), parent.box().maxWidth, builder.box().maxWidth);
  setBoxLP(LXB_CSS_PROPERTY_MAX_HEIGHT, LengthValue::makeNone(), parent.box().maxHeight, builder.box().maxHeight);

  // ---- Surround: margin / padding / inset (TRBL physical) -----------------
  struct SideProp {
    uint16_t id;
    int side;
  };
  const SideProp margins[4] = {
      {LXB_CSS_PROPERTY_MARGIN_TOP, kTop},
      {LXB_CSS_PROPERTY_MARGIN_RIGHT, kRight},
      {LXB_CSS_PROPERTY_MARGIN_BOTTOM, kBottom},
      {LXB_CSS_PROPERTY_MARGIN_LEFT, kLeft}};
  const SideProp paddings[4] = {
      {LXB_CSS_PROPERTY_PADDING_TOP, kTop},
      {LXB_CSS_PROPERTY_PADDING_RIGHT, kRight},
      {LXB_CSS_PROPERTY_PADDING_BOTTOM, kBottom},
      {LXB_CSS_PROPERTY_PADDING_LEFT, kLeft}};
  const SideProp insets[4] = {
      {LXB_CSS_PROPERTY_TOP, kTop},
      {LXB_CSS_PROPERTY_RIGHT, kRight},
      {LXB_CSS_PROPERTY_BOTTOM, kBottom},
      {LXB_CSS_PROPERTY_LEFT, kLeft}};

  for (const auto& m : margins) {
    LengthValue v = resolveLP(cascaded, m.id, ctx, LengthValue::makePx(0), parent.surround().margin[m.side], false, &present);
    if (present) builder.surround().margin[m.side] = v;
  }
  for (const auto& p : paddings) {
    LengthValue v = resolveLP(cascaded, p.id, ctx, LengthValue::makePx(0), parent.surround().padding[p.side], false, &present);
    if (present) builder.surround().padding[p.side] = v;
  }
  for (const auto& in : insets) {
    LengthValue v = resolveLP(cascaded, in.id, ctx, LengthValue::makeAuto(), parent.surround().inset[in.side], false, &present);
    if (present) builder.surround().inset[in.side] = v;
  }

  // ---- Inherited text details --------------------------------------------
  if (const auto* ta = declared<lxb_css_property_text_align_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_ALIGN)) {
    switch (wideFromType(ta->type)) {
    case Wide::Initial:
      builder.text().textAlign = TextAlign::Start;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().textAlign = mapTextAlign(ta->type);
      break;
    }
  }
  if (const auto* ta = declared<lxb_css_property_text_align_all_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_ALIGN_ALL)) {
    switch (wideFromType(ta->type)) {
    case Wide::Initial:
      builder.text().textAlign = TextAlign::Start;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().textAlign = mapTextAlign(ta->type);
      break;
    }
  }
  if (const auto* ws = declared<lxb_css_property_white_space_t>(
          cascaded,
          LXB_CSS_PROPERTY_WHITE_SPACE)) {
    switch (wideFromType(ws->type)) {
    case Wide::Initial:
      builder.text().whiteSpace = WhiteSpace::Normal;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().whiteSpace = mapWhiteSpace(ws->type);
      break;
    }
  }
  if (const auto* ti = declared<lxb_css_property_text_indent_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_INDENT)) {
    switch (wideFromType(ti->type)) {
    case Wide::Initial:
      builder.text().textIndent = LengthValue::makePx(0);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().textIndent = resolveLengthPercentage(ti->length, ctx);
      break;
    }
  }
  if (const auto* ls = declared<lxb_css_property_letter_spacing_t>(
          cascaded,
          LXB_CSS_PROPERTY_LETTER_SPACING)) {
    switch (wideFromType(ls->type)) {
    case Wide::Initial:
      builder.text().letterSpacing = LengthValue::makeAuto();
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().letterSpacing =
          ls->type == LXB_CSS_LETTER_SPACING__LENGTH
              ? LengthValue::makePx(resolveLengthPx(ls->length, ctx))
              : LengthValue::makeAuto();
      break;
    }
  }
  if (const auto* ws = declared<lxb_css_property_word_spacing_t>(
          cascaded,
          LXB_CSS_PROPERTY_WORD_SPACING)) {
    switch (wideFromType(ws->type)) {
    case Wide::Initial:
      builder.text().wordSpacing = LengthValue::makePx(0);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // inherited
    case Wide::None:
      builder.text().wordSpacing =
          ws->type == LXB_CSS_WORD_SPACING__LENGTH
              ? LengthValue::makePx(resolveLengthPx(ws->length, ctx))
              : LengthValue::makePx(0);
      break;
    }
  }

  // ---- Border edges -------------------------------------------------------
  const SideProp borders[4] = {
      {LXB_CSS_PROPERTY_BORDER_TOP, kTop},
      {LXB_CSS_PROPERTY_BORDER_RIGHT, kRight},
      {LXB_CSS_PROPERTY_BORDER_BOTTOM, kBottom},
      {LXB_CSS_PROPERTY_BORDER_LEFT, kLeft}};
  const SideProp borderColors[4] = {
      {LXB_CSS_PROPERTY_BORDER_TOP_COLOR, kTop},
      {LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR, kRight},
      {LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR, kBottom},
      {LXB_CSS_PROPERTY_BORDER_LEFT_COLOR, kLeft}};

  for (const auto& b : borders) {
    BorderEdge edge = resolveBorderEdge(cascaded, b.id, ctx, currentColor, parent.surround().border[b.side], &present);
    if (present) {
      builder.surround().border[b.side] = edge;
    }
  }
  for (const auto& c : borderColors) {
    resolveBorderColor(cascaded, c.id, currentColor, parent.surround().border[c.side], builder.surround().border[c.side]);
  }

  // ---- Visual: background-color, opacity, overflow ------------------------
  if (const auto* c = declared<lxb_css_property_background_color_t>(
          cascaded,
          LXB_CSS_PROPERTY_BACKGROUND_COLOR)) {
    Color out;
    switch (wideFromType(c->type)) {
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.visual().background = Color::transparent();
      break;
    case Wide::Inherit:
      builder.visual().background = parent.visual().background;
      break;
    case Wide::None:
      if (convColor(*c, currentColor, out)) builder.visual().background = out;
      break;
    }
  }
  if (const auto* o = declared<lxb_css_property_opacity_t>(
          cascaded,
          LXB_CSS_PROPERTY_OPACITY)) {
    if (o->type == LXB_CSS_VALUE__NUMBER) {
      builder.visual().opacity = static_cast<float>(o->u.number.num);
    } else if (o->type == LXB_CSS_VALUE__PERCENTAGE) {
      builder.visual().opacity = static_cast<float>(o->u.percentage.num) / 100.0f;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_x_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_X)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.visual().overflowX = parent.visual().overflowX;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.visual().overflowX = Overflow::Visible;
      break;
    case Wide::None:
      builder.visual().overflowX = mapOverflow(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_y_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_Y)) {
    // lexbor's *_type_t are all `unsigned int` and OVERFLOW_Y_* alias the same
    // LXB_CSS_VALUE_* ids as OVERFLOW_X_*, so mapOverflow handles both.
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.visual().overflowY = parent.visual().overflowY;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.visual().overflowY = Overflow::Visible;
      break;
    case Wide::None:
      builder.visual().overflowY = mapOverflow(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_inline_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_INLINE)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.visual().overflowX = parent.visual().overflowX;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.visual().overflowX = Overflow::Visible;
      break;
    case Wide::None:
      builder.visual().overflowX = mapOverflow(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_block_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_BLOCK)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.visual().overflowY = parent.visual().overflowY;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.visual().overflowY = Overflow::Visible;
      break;
    case Wide::None:
      builder.visual().overflowY = mapOverflow(d->type);
      break;
    }
  }

  return style;
}

void StyleResolver::resolveSubtree(lxb_dom_element_t* element,
                                   const ComputedStyle& parent,
                                   const ResolveContext& parentCtx) {
  ResolveContext outCtx;
  auto style = resolveElement(element, parent, parentCtx, outCtx);
  styles_[lxb_dom_interface_node(element)] = style;

  for (lxb_dom_node_t* child = lxb_dom_interface_node(element)->first_child;
       child != nullptr;
       child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      resolveSubtree(lxb_dom_interface_element(child), *style, outCtx);
    }
  }
}

void StyleResolver::resolveDocument(lxb_html_document_t* doc,
                                    const ResolveContext& rootCtx) {
  styles_.clear();
  heap_.clear();
  lxb_dom_document_t* dom = lxb_dom_interface_document(doc);
  lxb_dom_element_t* root = lxb_dom_document_element(dom);
  if (root == nullptr) {
    return;
  }
  ComputedStyle initial; // the inherited-root baseline (all initial values)
  resolveSubtree(root, initial, rootCtx);
}

const ComputedStyle* StyleResolver::styleFor(const lxb_dom_node_t* node) const {
  auto it = styles_.find(node);
  return it == styles_.end() ? nullptr : it->second;
}

} // namespace style
