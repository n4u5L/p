#include "style_props.h"

#include "computed_style.h"
#include "style_arena.h"

#include "lexbor/css/property.h"
#include "lexbor/css/property/const.h"
#include "lexbor/css/rule.h"
#include "lexbor/css/value.h"
#include "lexbor/css/value/const.h"

// 这里集中存放“声明值 -> computed value”的全部转换逻辑：
//   * 通用辅助（wide keyword、颜色转换、长度解析、shorthand 归一）；
//   * 每个属性一个 apply_* 函数（对应 lexbor 一属性一函数的风格）；
//   * 末尾的 kPropTable[] 把它们按 id/phase 组织成静态表。
// resolver 只负责按阶段遍历这张表，不再持有任何 per-property 分支。

namespace style {

namespace {

// ---------------------------------------------------------------------------
// 通用辅助
// ---------------------------------------------------------------------------

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

bool convColor(const lxb_css_value_color_t& c, const Color& currentColorValue,
               Color& out);

// margin/padding longhand 归属的 shorthand id；其它 length 属性无 shorthand 来源。
uint16_t shorthandOwnerLP(uint16_t id) {
  switch (id) {
  case LXB_CSS_PROPERTY_MARGIN_TOP:
  case LXB_CSS_PROPERTY_MARGIN_RIGHT:
  case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
  case LXB_CSS_PROPERTY_MARGIN_LEFT:
    return LXB_CSS_PROPERTY_MARGIN;
  case LXB_CSS_PROPERTY_PADDING_TOP:
  case LXB_CSS_PROPERTY_PADDING_RIGHT:
  case LXB_CSS_PROPERTY_PADDING_BOTTOM:
  case LXB_CSS_PROPERTY_PADDING_LEFT:
    return LXB_CSS_PROPERTY_PADDING;
  default:
    return 0;
  }
}

// 从四边 shorthand（margin/padding）按 CSS 1-2-3-4 值规则取出某条物理边。
const lxb_css_value_length_percentage_t* sideFromBox(
    const lxb_css_value_length_percentage_t& top,
    const lxb_css_value_length_percentage_t& right,
    const lxb_css_value_length_percentage_t& bottom,
    const lxb_css_value_length_percentage_t& left, uint16_t side) {
  switch (side) {
  case LXB_CSS_PROPERTY_MARGIN_TOP:
  case LXB_CSS_PROPERTY_PADDING_TOP:
    return &top;
  case LXB_CSS_PROPERTY_MARGIN_RIGHT:
  case LXB_CSS_PROPERTY_PADDING_RIGHT:
    return isUnsetSide(right) ? &top : &right;
  case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
  case LXB_CSS_PROPERTY_PADDING_BOTTOM:
    return isUnsetSide(bottom) ? &top : &bottom;
  case LXB_CSS_PROPERTY_MARGIN_LEFT:
  case LXB_CSS_PROPERTY_PADDING_LEFT:
    if (!isUnsetSide(left)) return &left;
    return isUnsetSide(right) ? &top : &right;
  default:
    return nullptr;
  }
}

// 取 canonical longhand `id` 的 <length-percentage> 声明值。
// margin/padding 的边可能来自 shorthand：longhand 与 owning shorthand 同时存在时
// 按 specificity 仲裁（相等优先显式 longhand），shorthand 胜出再拆出对应边。
const lxb_css_value_length_percentage_t* declaredLength(
    const DeclaredStyle& cascaded, uint16_t id) {
  const auto* lh = cascaded.value<lxb_css_value_length_percentage_t>(id);
  const uint16_t shId = shorthandOwnerLP(id);
  if (shId == 0) {
    return lh; // width/height/min/max/inset 等：没有 shorthand 来源。
  }

  const lxb_css_rule_declaration_t* shDecl = cascaded.declaration(shId);
  if (lh != nullptr &&
      (shDecl == nullptr ||
       cascaded.specificity(id) >= cascaded.specificity(shId))) {
    return lh;
  }
  if (shDecl == nullptr) {
    return lh; // 两者都没有 -> nullptr。
  }

  if (shId == LXB_CSS_PROPERTY_MARGIN) {
    const auto* m = static_cast<const lxb_css_property_margin_t*>(shDecl->u.user);
    return m == nullptr ? nullptr
                        : sideFromBox(m->top, m->right, m->bottom, m->left, id);
  }
  const auto* p = static_cast<const lxb_css_property_padding_t*>(shDecl->u.user);
  return p == nullptr ? nullptr
                      : sideFromBox(p->top, p->right, p->bottom, p->left, id);
}

// border 边的声明值：longhand border-top/... 缺省回退到 border shorthand；
// 两者都在时按 specificity 仲裁（相等优先 longhand）。
const lxb_css_property_border_t* declaredBorder(const DeclaredStyle& cascaded,
                                                uint16_t edgeId) {
  const auto* lh = cascaded.value<lxb_css_property_border_t>(edgeId);
  const auto* sh =
      cascaded.value<lxb_css_property_border_t>(LXB_CSS_PROPERTY_BORDER);
  if (lh != nullptr && sh != nullptr) {
    return cascaded.specificity(edgeId) >=
                   cascaded.specificity(LXB_CSS_PROPERTY_BORDER)
               ? lh
               : sh;
  }
  return lh != nullptr ? lh : sh;
}

// number-or-percentage 颜色通道 -> 0..255。
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

// lexbor color -> 具体 Color。wide keyword 返回 false，由调用方按继承规则处理。
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
    out = Color(channel8(c.u.rgb.r), channel8(c.u.rgb.g), channel8(c.u.rgb.b),
                alpha8(c.u.rgb.a));
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
    return false;
  default:
    // TODO：补齐 named colors、hsl/lab/lch。
    out = Color::black();
    return true;
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
    return kFontFamilySerif;
  case LXB_CSS_FONT_FAMILY_MONOSPACE:
  case LXB_CSS_FONT_FAMILY_UI_MONOSPACE:
    return kFontFamilyMonospace;
  case LXB_CSS_FONT_FAMILY_CURSIVE:
    return kFontFamilyCursive;
  case LXB_CSS_FONT_FAMILY_FANTASY:
    return kFontFamilyFantasy;
  case LXB_CSS_FONT_FAMILY_SYSTEM_UI:
    return kFontFamilySystemUi;
  case LXB_CSS_FONT_FAMILY_EMOJI:
    return kFontFamilyEmoji;
  case LXB_CSS_FONT_FAMILY_MATH:
    return kFontFamilyMath;
  case LXB_CSS_FONT_FAMILY_FANGSONG:
    return kFontFamilyFangsong;
  case LXB_CSS_FONT_FAMILY_UI_ROUNDED:
    return kFontFamilyUiRounded;
  case LXB_CSS_FONT_FAMILY_SANS_SERIF:
  case LXB_CSS_FONT_FAMILY_UI_SANS_SERIF:
  default:
    return kFontFamilySansSerif;
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

void resolveBorderEdgeValue(const lxb_css_property_border_t& b,
                            const ResolveContext& ctx,
                            const Color& currentColor, LengthValue& width,
                            lxb_css_border_type_t& styleKind, Color& color) {
  styleKind = b.style == LXB_CSS_VALUE__UNDEF ? LXB_CSS_BORDER_NONE : b.style;
  width = (styleKind == LXB_CSS_BORDER_NONE || styleKind == LXB_CSS_BORDER_HIDDEN)
              ? LengthValue::makePx(0.0f)
              : borderWidth(b.width, ctx);

  Color out;
  if (b.color.type == LXB_CSS_VALUE__UNDEF) {
    color = currentColor;
  } else if (convColor(b.color, currentColor, out)) {
    color = out;
  }
}

void resolveBorderEdge(const DeclaredStyle& cascaded, uint16_t id,
                       const ResolveContext& ctx, const Color& currentColor,
                       const LengthValue& parentWidth,
                       lxb_css_border_type_t parentStyle,
                       const Color& parentColor, LengthValue& width,
                       lxb_css_border_type_t& styleKind, Color& color) {
  const auto* b = declaredBorder(cascaded, id);
  if (b == nullptr) {
    return;
  }
  switch (wideFromType(b->style)) {
  case Wide::Inherit:
    width = parentWidth;
    styleKind = parentStyle;
    color = parentColor;
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    width = LengthValue::makePx(0.0f);
    styleKind = LXB_CSS_BORDER_NONE;
    color = currentColor;
    break;
  case Wide::None:
    resolveBorderEdgeValue(*b, ctx, currentColor, width, styleKind, color);
    break;
  }
}

void resolveBorderColor(const DeclaredStyle& cascaded, uint16_t id,
                        const Color& currentColor, const Color& parentColor,
                        Color& color) {
  const auto* c = cascaded.value<lxb_css_value_color_t>(id);
  if (c == nullptr) {
    return;
  }
  Color out;
  switch (wideFromType(c->type)) {
  case Wide::Inherit:
    color = parentColor;
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    color = currentColor;
    break;
  case Wide::None:
    if (convColor(*c, currentColor, out)) color = out;
    break;
  }
}

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

float resolveFontSizePx(const DeclaredStyle& cascaded, float parentFontSizePx,
                        const ResolveContext& ctx) {
  using FS = lxb_css_property_font_size_t;
  const FS* fs = cascaded.value<FS>(LXB_CSS_PROPERTY_FONT_SIZE);
  if (!fs) {
    return parentFontSizePx; // font-size 默认继承。
  }
  switch (wideFromType(fs->type)) {
  case Wide::Inherit:
  case Wide::Unset:
    return parentFontSizePx;
  case Wide::Initial:
    return 16.0f;
  case Wide::Revert:
    return parentFontSizePx;
  case Wide::None:
    break;
  }
  if (fs->type != LXB_CSS_VALUE__LENGTH && fs->type != LXB_CSS_VALUE__PERCENTAGE) {
    if (fs->type == LXB_CSS_FONT_SIZE_LARGER) return parentFontSizePx * 1.2f;
    if (fs->type == LXB_CSS_FONT_SIZE_SMALLER) return parentFontSizePx / 1.2f;
    return fontSizeKeywordPx(fs->type);
  }
  const lxb_css_value_length_percentage_t& lp = fs->length;
  if (lp.type == LXB_CSS_VALUE__PERCENTAGE) {
    return parentFontSizePx * static_cast<float>(lp.u.percentage.num) / 100.0f;
  }
  // font-size 上的 em/ex/ch 相对父字号，不是当前元素字号。
  ResolveContext c = ctx;
  c.fontSizePx = parentFontSizePx;
  c.parentFontSizePx = parentFontSizePx;
  c.exPx = parentFontSizePx * 0.5f;
  c.chPx = parentFontSizePx * 0.5f;
  return resolveLengthPx(lp.u.length, c, /*forFontSize=*/true);
}

// <length-percentage> 属性求值 + wide keyword + 继承。
// initialVal 是 initial 值；parentVal 仅 inherit 用。present 表示是否声明过。
LengthValue resolveLP(const DeclaredStyle& cascaded, uint16_t id,
                      const ResolveContext& ctx, LengthValue initialVal,
                      LengthValue parentVal, bool inheritedProp, bool* present) {
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
    return inheritedProp ? parentVal : initialVal;
  case Wide::None:
    return resolveLengthPercentage(*lp, ctx);
  }
  return initialVal;
}

// ---------------------------------------------------------------------------
// 阶段 0：writing context
// ---------------------------------------------------------------------------

void apply_direction(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_direction_t>(LXB_CSS_PROPERTY_DIRECTION);
  if (!d) return;
  switch (wideFromType(d->type)) {
  case Wide::Initial:
    ac.builder.SetDirection(LXB_CSS_DIRECTION_LTR);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break; // 继承：保留 inheritFrom() 已共享的父值。
  case Wide::None:
    ac.builder.SetDirection(d->type);
    break;
  }
}

void apply_writing_mode(ApplyContext& ac) {
  const auto* d = ac.declared.value<lxb_css_property_writing_mode_t>(
      LXB_CSS_PROPERTY_WRITING_MODE);
  if (!d) return;
  switch (wideFromType(d->type)) {
  case Wide::Initial:
    ac.builder.SetWritingMode(LXB_CSS_WRITING_MODE_HORIZONTAL_TB);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.SetWritingMode(d->type);
    break;
  }
}

// ---------------------------------------------------------------------------
// 阶段 1：color
// ---------------------------------------------------------------------------

void apply_color(ApplyContext& ac) {
  const auto* c =
      ac.declared.value<lxb_css_property_color_t>(LXB_CSS_PROPERTY_COLOR);
  if (!c) return;
  Color resolved = ac.parent.InheritedData().color;
  Color out;
  switch (wideFromType(c->type)) {
  case Wide::Initial:
    resolved = Color::black();
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    resolved = ac.parent.InheritedData().color;
    break;
  case Wide::None:
    // color 自身里的 currentColor 表示继承来的 color。
    if (convColor(*c, ac.parent.InheritedData().color, out)) resolved = out;
    break;
  }
  ac.builder.InheritedData().color = resolved;
}

// ---------------------------------------------------------------------------
// 阶段 2：font-size（始终运行，写回 ctx 供后续阶段/子元素使用）
// ---------------------------------------------------------------------------

void apply_font_size(ApplyContext& ac) {
  const float parentFontSizePx = ac.ctx.fontSizePx; // 进入时仍是父上下文。
  const float fontSizePx =
      resolveFontSizePx(ac.declared, parentFontSizePx, ac.ctx);
  ac.builder.Font().sizePx = fontSizePx;
  ac.ctx.parentFontSizePx = fontSizePx;
  ac.ctx.fontSizePx = fontSizePx;
  ac.ctx.exPx = fontSizePx * 0.5f; // TODO(M2)：QFontMetricsF::xHeight()。
  ac.ctx.chPx = fontSizePx * 0.5f; // TODO(M2)：字符 '0' advance。
  ac.ctx.lineHeightPx = fontSizePx * 1.2f;
}

// ---------------------------------------------------------------------------
// 阶段 3：font 属性
// ---------------------------------------------------------------------------

void apply_font_family(ApplyContext& ac) {
  const auto* ff = ac.declared.value<lxb_css_property_font_family_t>(
      LXB_CSS_PROPERTY_FONT_FAMILY);
  if (!ff || ff->first == nullptr) return;
  const lxb_css_property_family_name_t* name = ff->first;
  if (name->generic) {
    ac.builder.Font().family = genericFontFamily(name->u.type);
  } else {
    ac.builder.Font().family = ac.heap.internString(
        reinterpret_cast<const char*>(name->u.str.data), name->u.str.length);
  }
}

void apply_font_weight(ApplyContext& ac) {
  const auto* fw = ac.declared.value<lxb_css_property_font_weight_t>(
      LXB_CSS_PROPERTY_FONT_WEIGHT);
  if (!fw) return;
  switch (wideFromType(fw->type)) {
  case Wide::Initial:
    ac.builder.Font().weight = 400;
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    if (fw->type == LXB_CSS_FONT_WEIGHT_BOLD) {
      ac.builder.Font().weight = 700;
    } else if (fw->type == LXB_CSS_FONT_WEIGHT_BOLDER) {
      ac.builder.Font().weight = ac.parent.Font().weight < 600 ? 700 : 900;
    } else if (fw->type == LXB_CSS_FONT_WEIGHT_LIGHTER) {
      ac.builder.Font().weight = ac.parent.Font().weight >= 600 ? 400 : 100;
    } else if (fw->type == LXB_CSS_FONT_WEIGHT__NUMBER) {
      ac.builder.Font().weight = static_cast<int>(fw->number.num);
    } else {
      ac.builder.Font().weight = 400;
    }
    break;
  }
}

void apply_font_style(ApplyContext& ac) {
  const auto* fs =
      ac.declared.value<lxb_css_property_font_style_t>(LXB_CSS_PROPERTY_FONT_STYLE);
  if (!fs) return;
  switch (wideFromType(fs->type)) {
  case Wide::Initial:
    ac.builder.Font().style = LXB_CSS_FONT_STYLE_NORMAL;
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.Font().style = fs->type;
    break;
  }
}

void apply_font_stretch(ApplyContext& ac) {
  const auto* fst = ac.declared.value<lxb_css_property_font_stretch_t>(
      LXB_CSS_PROPERTY_FONT_STRETCH);
  if (!fst) return;
  switch (wideFromType(fst->type)) {
  case Wide::Initial:
    ac.builder.Font().stretchPercent = 100.0f;
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    if (fst->type == LXB_CSS_FONT_STRETCH__PERCENTAGE) {
      ac.builder.Font().stretchPercent = static_cast<float>(fst->percentage.num);
    } else {
      ac.builder.Font().stretchPercent = fontStretchPercent(fst->type);
    }
    break;
  }
}

// ---------------------------------------------------------------------------
// 阶段 4：line-height（写回 ctx.lineHeightPx）
// ---------------------------------------------------------------------------

void apply_line_height(ApplyContext& ac) {
  const auto* lh = ac.declared.value<lxb_css_property_line_height_t>(
      LXB_CSS_PROPERTY_LINE_HEIGHT);
  if (!lh) return;
  const float fontSizePx = ac.ctx.fontSizePx;
  LineHeight value = ac.parent.InheritedData().lineHeight;
  switch (wideFromType(lh->type)) {
  case Wide::Initial:
    value = LineHeight{};
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    value = ac.parent.InheritedData().lineHeight;
    break;
  case Wide::None:
    if (lh->type == LXB_CSS_VALUE__NUMBER) {
      value.kind = LineHeight::Kind::Number;
      value.value = static_cast<float>(lh->u.number.num);
      ac.ctx.lineHeightPx = fontSizePx * value.value;
    } else if (lh->type == LXB_CSS_VALUE__PERCENTAGE) {
      value.kind = LineHeight::Kind::LengthPx;
      value.value = fontSizePx * static_cast<float>(lh->u.percentage.num) / 100.0f;
      ac.ctx.lineHeightPx = value.value;
    } else if (lh->type == LXB_CSS_VALUE__LENGTH) {
      value.kind = LineHeight::Kind::LengthPx;
      value.value = resolveLengthPx(lh->u.length, ac.ctx);
      ac.ctx.lineHeightPx = value.value;
    }
    break;
  }
  ac.builder.InheritedData().lineHeight = value;
}

// ---------------------------------------------------------------------------
// 阶段 5：Normal —— 枚举类
// ---------------------------------------------------------------------------

void apply_display(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_display_t>(LXB_CSS_PROPERTY_DISPLAY);
  if (!d) return;
  switch (wideFromType(d->a)) {
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    ac.builder.SetDisplay(LXB_CSS_DISPLAY_INLINE);
    break;
  case Wide::Inherit:
    ac.builder.SetDisplay(ac.parent.Display());
    break;
  case Wide::None:
    ac.builder.SetDisplay(d->a);
    break;
  }
}

void apply_position(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_position_t>(LXB_CSS_PROPERTY_POSITION);
  if (!d) return;
  if (wideFromType(d->type) == Wide::Inherit) {
    ac.builder.SetPosition(ac.parent.Position());
  } else if (wideFromType(d->type) == Wide::None) {
    ac.builder.SetPosition(d->type);
  } // initial/unset/revert -> static（默认值已是 static）。
}

void apply_box_sizing(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_box_sizing_t>(LXB_CSS_PROPERTY_BOX_SIZING);
  if (!d) return;
  if (wideFromType(d->type) == Wide::Inherit) {
    ac.builder.BoxData().boxSizing = ac.parent.BoxData().boxSizing;
  } else if (wideFromType(d->type) == Wide::None) {
    ac.builder.BoxData().boxSizing = d->type;
  }
}

void apply_float(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_float_t>(LXB_CSS_PROPERTY_FLOAT);
  if (!d) return;
  if (wideFromType(d->type) == Wide::Inherit) {
    ac.builder.VisualData().floating = ac.parent.VisualData().floating;
  } else if (wideFromType(d->type) == Wide::None) {
    ac.builder.VisualData().floating = d->type;
  }
}

void apply_clear(ApplyContext& ac) {
  const auto* d =
      ac.declared.value<lxb_css_property_clear_t>(LXB_CSS_PROPERTY_CLEAR);
  if (!d) return;
  const Wide w = wideFromType(d->type);
  if (w == Wide::Inherit) {
    ac.builder.SetClear(ac.parent.Clear());
  } else if (w == Wide::Initial || w == Wide::Unset || w == Wide::Revert) {
    ac.builder.SetClear(LXB_CSS_CLEAR_NONE);
  } else {
    ac.builder.SetClear(d->type);
  }
}

void apply_z_index(ApplyContext& ac) {
  const auto* z =
      ac.declared.value<lxb_css_property_z_index_t>(LXB_CSS_PROPERTY_Z_INDEX);
  if (!z) return;
  switch (wideFromType(z->type)) {
  case Wide::Inherit:
    ac.builder.BoxData().zIndex = ac.parent.BoxData().zIndex;
    ac.builder.BoxData().zIndexAuto = ac.parent.BoxData().zIndexAuto;
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    ac.builder.BoxData().zIndex = 0;
    ac.builder.BoxData().zIndexAuto = true;
    break;
  case Wide::None:
    if (z->type == LXB_CSS_Z_INDEX__INTEGER) {
      ac.builder.BoxData().zIndex = static_cast<int>(z->integer.num);
      ac.builder.BoxData().zIndexAuto = false;
    } else {
      ac.builder.BoxData().zIndex = 0;
      ac.builder.BoxData().zIndexAuto = true;
    }
    break;
  }
}

void apply_visibility(ApplyContext& ac) {
  const auto* d = ac.declared.value<lxb_css_property_visibility_t>(
      LXB_CSS_PROPERTY_VISIBILITY);
  if (!d) return;
  switch (wideFromType(d->type)) {
  case Wide::Initial:
    ac.builder.SetVisibility(LXB_CSS_VISIBILITY_VISIBLE);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.SetVisibility(d->type);
    break;
  }
}

// ---------------------------------------------------------------------------
// 阶段 5：Normal —— box 尺寸（length-percentage scalar，成员指针寻址）
// ---------------------------------------------------------------------------

using BoxLPSlot = LengthValue StyleBoxData::*;

void applyBoxLP(ApplyContext& ac, uint16_t id, LengthValue initial,
                BoxLPSlot slot) {
  bool present = false;
  LengthValue v = resolveLP(ac.declared, id, ac.ctx, initial,
                            ac.parent.BoxData().*slot, false, &present);
  if (present) ac.builder.BoxData().*slot = v;
}

void apply_width(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_WIDTH, LengthValue::makeAuto(),
             &StyleBoxData::width);
}
void apply_height(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_HEIGHT, LengthValue::makeAuto(),
             &StyleBoxData::height);
}
void apply_min_width(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_MIN_WIDTH, LengthValue::makePx(0),
             &StyleBoxData::minWidth);
}
void apply_min_height(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_MIN_HEIGHT, LengthValue::makePx(0),
             &StyleBoxData::minHeight);
}
void apply_max_width(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_MAX_WIDTH, LengthValue::makeNone(),
             &StyleBoxData::maxWidth);
}
void apply_max_height(ApplyContext& ac) {
  applyBoxLP(ac, LXB_CSS_PROPERTY_MAX_HEIGHT, LengthValue::makeNone(),
             &StyleBoxData::maxHeight);
}

// margin / padding（BoxData 数组）、inset（SurroundData 数组）。
void applyMarginSide(ApplyContext& ac, uint16_t id, int side) {
  bool present = false;
  LengthValue v = resolveLP(ac.declared, id, ac.ctx, LengthValue::makePx(0),
                            ac.parent.BoxData().margin[side], false, &present);
  if (present) ac.builder.BoxData().margin[side] = v;
}
void applyPaddingSide(ApplyContext& ac, uint16_t id, int side) {
  bool present = false;
  LengthValue v = resolveLP(ac.declared, id, ac.ctx, LengthValue::makePx(0),
                            ac.parent.BoxData().padding[side], false, &present);
  if (present) ac.builder.BoxData().padding[side] = v;
}
void applyInsetSide(ApplyContext& ac, uint16_t id, int side) {
  bool present = false;
  LengthValue v = resolveLP(ac.declared, id, ac.ctx, LengthValue::makeAuto(),
                            ac.parent.SurroundData().inset[side], false, &present);
  if (present) ac.builder.SurroundData().inset[side] = v;
}

void apply_margin_top(ApplyContext& ac) {
  applyMarginSide(ac, LXB_CSS_PROPERTY_MARGIN_TOP, kTop);
}
void apply_margin_right(ApplyContext& ac) {
  applyMarginSide(ac, LXB_CSS_PROPERTY_MARGIN_RIGHT, kRight);
}
void apply_margin_bottom(ApplyContext& ac) {
  applyMarginSide(ac, LXB_CSS_PROPERTY_MARGIN_BOTTOM, kBottom);
}
void apply_margin_left(ApplyContext& ac) {
  applyMarginSide(ac, LXB_CSS_PROPERTY_MARGIN_LEFT, kLeft);
}
void apply_padding_top(ApplyContext& ac) {
  applyPaddingSide(ac, LXB_CSS_PROPERTY_PADDING_TOP, kTop);
}
void apply_padding_right(ApplyContext& ac) {
  applyPaddingSide(ac, LXB_CSS_PROPERTY_PADDING_RIGHT, kRight);
}
void apply_padding_bottom(ApplyContext& ac) {
  applyPaddingSide(ac, LXB_CSS_PROPERTY_PADDING_BOTTOM, kBottom);
}
void apply_padding_left(ApplyContext& ac) {
  applyPaddingSide(ac, LXB_CSS_PROPERTY_PADDING_LEFT, kLeft);
}
void apply_inset_top(ApplyContext& ac) {
  applyInsetSide(ac, LXB_CSS_PROPERTY_TOP, kTop);
}
void apply_inset_right(ApplyContext& ac) {
  applyInsetSide(ac, LXB_CSS_PROPERTY_RIGHT, kRight);
}
void apply_inset_bottom(ApplyContext& ac) {
  applyInsetSide(ac, LXB_CSS_PROPERTY_BOTTOM, kBottom);
}
void apply_inset_left(ApplyContext& ac) {
  applyInsetSide(ac, LXB_CSS_PROPERTY_LEFT, kLeft);
}

// ---------------------------------------------------------------------------
// 阶段 5：Normal —— 继承 text 细节
// ---------------------------------------------------------------------------

void applyTextAlign(ApplyContext& ac, uint16_t id) {
  const auto* ta = ac.declared.value<lxb_css_property_text_align_t>(id);
  if (!ta) return;
  switch (wideFromType(ta->type)) {
  case Wide::Initial:
    ac.builder.SetTextAlign(LXB_CSS_TEXT_ALIGN_START);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.SetTextAlign(ta->type);
    break;
  }
}
void apply_text_align(ApplyContext& ac) {
  applyTextAlign(ac, LXB_CSS_PROPERTY_TEXT_ALIGN);
}
void apply_text_align_all(ApplyContext& ac) {
  applyTextAlign(ac, LXB_CSS_PROPERTY_TEXT_ALIGN_ALL);
}

void apply_white_space(ApplyContext& ac) {
  const auto* ws = ac.declared.value<lxb_css_property_white_space_t>(
      LXB_CSS_PROPERTY_WHITE_SPACE);
  if (!ws) return;
  switch (wideFromType(ws->type)) {
  case Wide::Initial:
    ac.builder.SetWhiteSpace(LXB_CSS_WHITE_SPACE_NORMAL);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.SetWhiteSpace(ws->type);
    break;
  }
}

void apply_text_indent(ApplyContext& ac) {
  const auto* ti = ac.declared.value<lxb_css_property_text_indent_t>(
      LXB_CSS_PROPERTY_TEXT_INDENT);
  if (!ti) return;
  switch (wideFromType(ti->type)) {
  case Wide::Initial:
    ac.builder.RareInheritedData().textIndent = LengthValue::makePx(0);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.RareInheritedData().textIndent =
        resolveLengthPercentage(ti->length, ac.ctx);
    break;
  }
}

void apply_letter_spacing(ApplyContext& ac) {
  const auto* ls = ac.declared.value<lxb_css_property_letter_spacing_t>(
      LXB_CSS_PROPERTY_LETTER_SPACING);
  if (!ls) return;
  switch (wideFromType(ls->type)) {
  case Wide::Initial:
    ac.builder.InheritedData().letterSpacing = LengthValue::makeAuto();
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.InheritedData().letterSpacing =
        ls->type == LXB_CSS_LETTER_SPACING__LENGTH
            ? LengthValue::makePx(resolveLengthPx(ls->length, ac.ctx))
            : LengthValue::makeAuto();
    break;
  }
}

void apply_word_spacing(ApplyContext& ac) {
  const auto* ws = ac.declared.value<lxb_css_property_word_spacing_t>(
      LXB_CSS_PROPERTY_WORD_SPACING);
  if (!ws) return;
  switch (wideFromType(ws->type)) {
  case Wide::Initial:
    ac.builder.InheritedData().wordSpacing = LengthValue::makePx(0);
    break;
  case Wide::Inherit:
  case Wide::Unset:
  case Wide::Revert:
    break;
  case Wide::None:
    ac.builder.InheritedData().wordSpacing =
        ws->type == LXB_CSS_WORD_SPACING__LENGTH
            ? LengthValue::makePx(resolveLengthPx(ws->length, ac.ctx))
            : LengthValue::makePx(0);
    break;
  }
}

// ---------------------------------------------------------------------------
// 阶段 5：Normal —— border 边与 border color
// ---------------------------------------------------------------------------

void applyBorderEdge(ApplyContext& ac, uint16_t id, int side) {
  resolveBorderEdge(ac.declared, id, ac.ctx, ac.currentColor,
                    ac.parent.BoxData().borderWidth[side],
                    ac.parent.BoxData().borderStyle[side],
                    ac.parent.SurroundData().borderColor[side],
                    ac.builder.BoxData().borderWidth[side],
                    ac.builder.BoxData().borderStyle[side],
                    ac.builder.SurroundData().borderColor[side]);
}
void apply_border_top(ApplyContext& ac) {
  applyBorderEdge(ac, LXB_CSS_PROPERTY_BORDER_TOP, kTop);
}
void apply_border_right(ApplyContext& ac) {
  applyBorderEdge(ac, LXB_CSS_PROPERTY_BORDER_RIGHT, kRight);
}
void apply_border_bottom(ApplyContext& ac) {
  applyBorderEdge(ac, LXB_CSS_PROPERTY_BORDER_BOTTOM, kBottom);
}
void apply_border_left(ApplyContext& ac) {
  applyBorderEdge(ac, LXB_CSS_PROPERTY_BORDER_LEFT, kLeft);
}

void applyBorderColor(ApplyContext& ac, uint16_t id, int side) {
  resolveBorderColor(ac.declared, id, ac.currentColor,
                     ac.parent.SurroundData().borderColor[side],
                     ac.builder.SurroundData().borderColor[side]);
}
void apply_border_top_color(ApplyContext& ac) {
  applyBorderColor(ac, LXB_CSS_PROPERTY_BORDER_TOP_COLOR, kTop);
}
void apply_border_right_color(ApplyContext& ac) {
  applyBorderColor(ac, LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR, kRight);
}
void apply_border_bottom_color(ApplyContext& ac) {
  applyBorderColor(ac, LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR, kBottom);
}
void apply_border_left_color(ApplyContext& ac) {
  applyBorderColor(ac, LXB_CSS_PROPERTY_BORDER_LEFT_COLOR, kLeft);
}

// ---------------------------------------------------------------------------
// 阶段 5：Normal —— background / svg / overflow
// ---------------------------------------------------------------------------

void apply_background_color(ApplyContext& ac) {
  const auto* c = ac.declared.value<lxb_css_property_background_color_t>(
      LXB_CSS_PROPERTY_BACKGROUND_COLOR);
  if (!c) return;
  Color out;
  switch (wideFromType(c->type)) {
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    ac.builder.BackgroundData().backgroundColor = Color::transparent();
    break;
  case Wide::Inherit:
    ac.builder.BackgroundData().backgroundColor =
        ac.parent.BackgroundData().backgroundColor;
    break;
  case Wide::None:
    if (convColor(*c, ac.currentColor, out)) {
      ac.builder.BackgroundData().backgroundColor = out;
    }
    break;
  }
}

void apply_opacity(ApplyContext& ac) {
  const auto* o =
      ac.declared.value<lxb_css_property_opacity_t>(LXB_CSS_PROPERTY_OPACITY);
  if (!o) return;
  if (o->type == LXB_CSS_VALUE__NUMBER) {
    ac.builder.SvgData().opacity = static_cast<float>(o->u.number.num);
  } else if (o->type == LXB_CSS_VALUE__PERCENTAGE) {
    ac.builder.SvgData().opacity =
        static_cast<float>(o->u.percentage.num) / 100.0f;
  }
}

void applyOverflowX(ApplyContext& ac, uint16_t id) {
  const auto* d = ac.declared.value<lxb_css_property_overflow_x_t>(id);
  if (!d) return;
  switch (wideFromType(d->type)) {
  case Wide::Inherit:
    ac.builder.SetOverflowX(ac.parent.OverflowX());
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    ac.builder.SetOverflowX(LXB_CSS_OVERFLOW_X_VISIBLE);
    break;
  case Wide::None:
    ac.builder.SetOverflowX(d->type);
    break;
  }
}
void applyOverflowY(ApplyContext& ac, uint16_t id) {
  const auto* d = ac.declared.value<lxb_css_property_overflow_y_t>(id);
  if (!d) return;
  switch (wideFromType(d->type)) {
  case Wide::Inherit:
    ac.builder.SetOverflowY(ac.parent.OverflowY());
    break;
  case Wide::Initial:
  case Wide::Unset:
  case Wide::Revert:
    ac.builder.SetOverflowY(LXB_CSS_OVERFLOW_Y_VISIBLE);
    break;
  case Wide::None:
    ac.builder.SetOverflowY(d->type);
    break;
  }
}
void apply_overflow_x(ApplyContext& ac) {
  applyOverflowX(ac, LXB_CSS_PROPERTY_OVERFLOW_X);
}
void apply_overflow_y(ApplyContext& ac) {
  applyOverflowY(ac, LXB_CSS_PROPERTY_OVERFLOW_Y);
}
void apply_overflow_inline(ApplyContext& ac) {
  applyOverflowX(ac, LXB_CSS_PROPERTY_OVERFLOW_INLINE);
}
void apply_overflow_block(ApplyContext& ac) {
  applyOverflowY(ac, LXB_CSS_PROPERTY_OVERFLOW_BLOCK);
}

// ---------------------------------------------------------------------------
// 静态表。Normal 阶段内顺序有意义：border 边在 border-*-color 之前，
// 以便显式 border-*-color 覆盖 border shorthand 带来的颜色。
// ---------------------------------------------------------------------------

const PropDesc kPropTable[] = {
    // 阶段 0：writing context
    {LXB_CSS_PROPERTY_DIRECTION, true, Phase::WritingMode, apply_direction},
    {LXB_CSS_PROPERTY_WRITING_MODE, true, Phase::WritingMode, apply_writing_mode},
    // 阶段 1：color
    {LXB_CSS_PROPERTY_COLOR, true, Phase::Color, apply_color},
    // 阶段 2：font-size
    {LXB_CSS_PROPERTY_FONT_SIZE, true, Phase::FontSize, apply_font_size},
    // 阶段 3：font
    {LXB_CSS_PROPERTY_FONT_FAMILY, true, Phase::Font, apply_font_family},
    {LXB_CSS_PROPERTY_FONT_WEIGHT, true, Phase::Font, apply_font_weight},
    {LXB_CSS_PROPERTY_FONT_STYLE, true, Phase::Font, apply_font_style},
    {LXB_CSS_PROPERTY_FONT_STRETCH, true, Phase::Font, apply_font_stretch},
    // 阶段 4：line-height
    {LXB_CSS_PROPERTY_LINE_HEIGHT, true, Phase::LineHeight, apply_line_height},
    // 阶段 5：Normal —— 枚举
    {LXB_CSS_PROPERTY_DISPLAY, false, Phase::Normal, apply_display},
    {LXB_CSS_PROPERTY_POSITION, false, Phase::Normal, apply_position},
    {LXB_CSS_PROPERTY_BOX_SIZING, false, Phase::Normal, apply_box_sizing},
    {LXB_CSS_PROPERTY_FLOAT, false, Phase::Normal, apply_float},
    {LXB_CSS_PROPERTY_CLEAR, false, Phase::Normal, apply_clear},
    {LXB_CSS_PROPERTY_Z_INDEX, false, Phase::Normal, apply_z_index},
    {LXB_CSS_PROPERTY_VISIBILITY, true, Phase::Normal, apply_visibility},
    // 阶段 5：Normal —— box 尺寸
    {LXB_CSS_PROPERTY_WIDTH, false, Phase::Normal, apply_width},
    {LXB_CSS_PROPERTY_HEIGHT, false, Phase::Normal, apply_height},
    {LXB_CSS_PROPERTY_MIN_WIDTH, false, Phase::Normal, apply_min_width},
    {LXB_CSS_PROPERTY_MIN_HEIGHT, false, Phase::Normal, apply_min_height},
    {LXB_CSS_PROPERTY_MAX_WIDTH, false, Phase::Normal, apply_max_width},
    {LXB_CSS_PROPERTY_MAX_HEIGHT, false, Phase::Normal, apply_max_height},
    // 阶段 5：Normal —— margin / padding / inset
    {LXB_CSS_PROPERTY_MARGIN_TOP, false, Phase::Normal, apply_margin_top},
    {LXB_CSS_PROPERTY_MARGIN_RIGHT, false, Phase::Normal, apply_margin_right},
    {LXB_CSS_PROPERTY_MARGIN_BOTTOM, false, Phase::Normal, apply_margin_bottom},
    {LXB_CSS_PROPERTY_MARGIN_LEFT, false, Phase::Normal, apply_margin_left},
    {LXB_CSS_PROPERTY_PADDING_TOP, false, Phase::Normal, apply_padding_top},
    {LXB_CSS_PROPERTY_PADDING_RIGHT, false, Phase::Normal, apply_padding_right},
    {LXB_CSS_PROPERTY_PADDING_BOTTOM, false, Phase::Normal, apply_padding_bottom},
    {LXB_CSS_PROPERTY_PADDING_LEFT, false, Phase::Normal, apply_padding_left},
    {LXB_CSS_PROPERTY_TOP, false, Phase::Normal, apply_inset_top},
    {LXB_CSS_PROPERTY_RIGHT, false, Phase::Normal, apply_inset_right},
    {LXB_CSS_PROPERTY_BOTTOM, false, Phase::Normal, apply_inset_bottom},
    {LXB_CSS_PROPERTY_LEFT, false, Phase::Normal, apply_inset_left},
    // 阶段 5：Normal —— 继承 text 细节
    {LXB_CSS_PROPERTY_TEXT_ALIGN, true, Phase::Normal, apply_text_align},
    {LXB_CSS_PROPERTY_TEXT_ALIGN_ALL, true, Phase::Normal, apply_text_align_all},
    {LXB_CSS_PROPERTY_WHITE_SPACE, true, Phase::Normal, apply_white_space},
    {LXB_CSS_PROPERTY_TEXT_INDENT, true, Phase::Normal, apply_text_indent},
    {LXB_CSS_PROPERTY_LETTER_SPACING, true, Phase::Normal, apply_letter_spacing},
    {LXB_CSS_PROPERTY_WORD_SPACING, true, Phase::Normal, apply_word_spacing},
    // 阶段 5：Normal —— border 边（必须在 border-*-color 之前）
    {LXB_CSS_PROPERTY_BORDER_TOP, false, Phase::Normal, apply_border_top},
    {LXB_CSS_PROPERTY_BORDER_RIGHT, false, Phase::Normal, apply_border_right},
    {LXB_CSS_PROPERTY_BORDER_BOTTOM, false, Phase::Normal, apply_border_bottom},
    {LXB_CSS_PROPERTY_BORDER_LEFT, false, Phase::Normal, apply_border_left},
    {LXB_CSS_PROPERTY_BORDER_TOP_COLOR, false, Phase::Normal,
     apply_border_top_color},
    {LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR, false, Phase::Normal,
     apply_border_right_color},
    {LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR, false, Phase::Normal,
     apply_border_bottom_color},
    {LXB_CSS_PROPERTY_BORDER_LEFT_COLOR, false, Phase::Normal,
     apply_border_left_color},
    // 阶段 5：Normal —— background / svg / overflow
    {LXB_CSS_PROPERTY_BACKGROUND_COLOR, false, Phase::Normal,
     apply_background_color},
    {LXB_CSS_PROPERTY_OPACITY, false, Phase::Normal, apply_opacity},
    {LXB_CSS_PROPERTY_OVERFLOW_X, false, Phase::Normal, apply_overflow_x},
    {LXB_CSS_PROPERTY_OVERFLOW_Y, false, Phase::Normal, apply_overflow_y},
    {LXB_CSS_PROPERTY_OVERFLOW_INLINE, false, Phase::Normal,
     apply_overflow_inline},
    {LXB_CSS_PROPERTY_OVERFLOW_BLOCK, false, Phase::Normal,
     apply_overflow_block},
};

} // namespace

const PropDesc* propDescBegin() {
  return kPropTable;
}

const PropDesc* propDescEnd() {
  return kPropTable + (sizeof(kPropTable) / sizeof(kPropTable[0]));
}

} // namespace style
