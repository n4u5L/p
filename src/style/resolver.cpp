#include "resolver.h"

#include <new>

#include "property_meta.h"

#include "lexbor/core/dobject.h"
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

// 取 canonical property `id` 对应的 typed value。
// 如果没有声明在级联中胜出，则返回 nullptr。
// 易错点：lexbor 的 declaration union 成员最终都别名到 u.user。
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
  width = (styleKind == LXB_CSS_BORDER_NONE ||
           styleKind == LXB_CSS_BORDER_HIDDEN)
              ? LengthValue::makePx(0.0f)
              : borderWidth(b.width, ctx);

  Color out;
  if (b.color.type == LXB_CSS_VALUE__UNDEF) {
    color = currentColor;
  } else if (convColor(b.color, currentColor, out)) {
    color = out;
  }
}

void resolveBorderEdge(const CascadedStyle& cascaded, uint16_t id,
                       const ResolveContext& ctx, const Color& currentColor,
                       const LengthValue& parentWidth,
                       lxb_css_border_type_t parentStyle,
                       const Color& parentColor, LengthValue& width,
                       lxb_css_border_type_t& styleKind, Color& color,
                       bool* present) {
  const auto* b = declared<lxb_css_property_border_t>(cascaded, id);
  *present = b != nullptr;
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

void resolveBorderColor(const CascadedStyle& cascaded, uint16_t id,
                        const Color& currentColor, const Color& parentColor,
                        Color& color) {
  const auto* c = declared<lxb_css_value_color_t>(cascaded, id);
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

// 将 lexbor 的 number-or-percentage 颜色通道转换到 0..255。
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

// 把 lexbor color 解析成具体 Color。
// currentColorValue 是元素已经算好的 color，用于非 color 属性里的 currentColor。
// 如果值是 inherit/initial 等 wide keyword，返回 false，由调用方处理。
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
    return false; // wide keyword，由调用方按属性继承规则处理。
  default:
    // TODO：补齐 named colors、hsl/lab/lch 等颜色表。
    out = Color::black();
    return true;
  }
}

// font-size 关键字（medium/large/...）转 px，当前以 16px medium 为基准。
// TODO：接入真实字体/缩放配置后，这里不能再写死 16px 基准。
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

// 将 font-size 解析成 px。percentage/em 都相对父元素 font-size。
float resolveFontSizePx(const CascadedStyle& cascaded, float parentFontSizePx,
                        const ResolveContext& ctx) {
  using FS = lxb_css_property_font_size_t; // lexbor 的 font-size typed value。
  const FS* fs = declared<FS>(cascaded, LXB_CSS_PROPERTY_FONT_SIZE);
  if (!fs) {
    return parentFontSizePx; // font-size 默认继承。
  }
  switch (wideFromType(fs->type)) {
  case Wide::Inherit:
  case Wide::Unset: // font-size 是继承属性，unset 等价 inherit。
    return parentFontSizePx;
  case Wide::Initial:
    return 16.0f;
  case Wide::Revert:
    return parentFontSizePx; // M1：revert 暂按 unset 处理。
  case Wide::None:
    break;
  }
  // 绝对/相对字号关键字也放在 fs->type 里。
  if (fs->type != LXB_CSS_VALUE__LENGTH && fs->type != LXB_CSS_VALUE__PERCENTAGE) {
    if (fs->type == LXB_CSS_FONT_SIZE_LARGER) return parentFontSizePx * 1.2f;
    if (fs->type == LXB_CSS_FONT_SIZE_SMALLER) return parentFontSizePx / 1.2f;
    return fontSizeKeywordPx(fs->type);
  }
  const lxb_css_value_length_percentage_t& lp = fs->length;
  if (lp.type == LXB_CSS_VALUE__PERCENTAGE) {
    return parentFontSizePx * static_cast<float>(lp.u.percentage.num) / 100.0f;
  }
  // length：font-size 上的 em/ex/ch 相对父元素字号，不是当前元素字号。
  // 易错点：传入的 ctx 来自父元素；ctx.fontSizePx 已经是父字号，
  // 但 ctx.parentFontSizePx 是祖父字号，所以这里把两者都钉到父字号。
  ResolveContext c = ctx;
  c.fontSizePx = parentFontSizePx;
  c.parentFontSizePx = parentFontSizePx;
  c.exPx = parentFontSizePx * 0.5f;
  c.chPx = parentFontSizePx * 0.5f;
  return resolveLengthPx(lp.u.length, c, /*forFontSize=*/true);
}

// 解析 <length-percentage> 属性，同时处理 wide keyword 和继承。
// initialVal 是属性 initial 值；parentVal 是父元素 computed 值，仅 inherit 使用。
// 非继承属性在 initial/unset 两种路径都传 initial。
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
    return inheritedProp ? parentVal : initialVal; // M1：revert 暂按 unset 处理。
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
    return inheritedProp ? parentVal : initialVal; // M1：revert 暂按 unset 处理。
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

enum NodeStateFlag : uint8_t {
  kNodeSelfDirty = 1 << 0,
  kNodeSubtreeDirty = 1 << 1,
  kNodeChildDirty = 1 << 2,
};

StyleResolver::StyleResolver() {
  nodeStates_ = lexbor_dobject_create();
  if (nodeStates_ == nullptr ||
      lexbor_dobject_init(nodeStates_, 1024, sizeof(NodeState)) != LXB_STATUS_OK) {
    nodeStates_ = lexbor_dobject_destroy(nodeStates_, true);
    throw std::bad_alloc();
  }
}

StyleResolver::~StyleResolver() {
  nodeStates_ = lexbor_dobject_destroy(nodeStates_, true);
}

void StyleResolver::beginDocument(const ResolveContext& rootCtx) {
  if (nodeStates_ != nullptr) {
    lexbor_dobject_clean(nodeStates_);
  }
  ++generation_;
  if (generation_ == 0) {
    generation_ = 1;
  }
  heap_.clear();
  rootCtx_ = rootCtx;
  initialStyle_ = ComputedStyle{};
}

ComputedStyle*
StyleResolver::resolveElement(lxb_dom_element_t* element,
                              const ComputedStyle& parent,
                              const ResolveContext& parentCtx,
                              ResolveContext& outCtx) {
  ComputedStyle* style = heap_.create<ComputedStyle>();
  style->inheritFrom(parent);
  ComputedStyleBuilder builder(*style, heap_);
  CascadedStyle cascaded = cascadedStyleNormalizer_.collect(element);

  // ---- 阶段 0：writing-mode、direction（逻辑方向转物理边的输入） ----
  if (const auto* d = declared<lxb_css_property_direction_t>(
          cascaded,
          LXB_CSS_PROPERTY_DIRECTION)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.SetDirection(LXB_CSS_DIRECTION_LTR);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.SetDirection(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_writing_mode_t>(
          cascaded,
          LXB_CSS_PROPERTY_WRITING_MODE)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.SetWritingMode(LXB_CSS_WRITING_MODE_HORIZONTAL_TB);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break;
    case Wide::None:
      builder.SetWritingMode(d->type);
      break;
    }
  }

  // ---- 阶段 1：color（currentColor 的来源） -------------------------------
  {
    Color resolved = parent.InheritedData().color; // 默认继承。
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
        resolved = parent.InheritedData().color;
        break;
      case Wide::None:
        // 易错点：在 color 属性自身里，currentColor 表示继承来的 color。
        if (convColor(*c, parent.InheritedData().color, out)) resolved = out;
        break;
      }
    }
    builder.InheritedData().color = resolved;
  }
  const Color currentColor = style->InheritedData().color;

  // ---- 阶段 2：font-size（em/ex/ch 的基准） -------------------------------
  const float fontSizePx =
      resolveFontSizePx(cascaded, parentCtx.fontSizePx, parentCtx);
  builder.Font().sizePx = fontSizePx;

  // 构建当前元素后续解析长度时使用的上下文。
  outCtx = parentCtx;
  outCtx.parentFontSizePx = fontSizePx; // 子元素解析 em/font-size 的父字号。
  outCtx.fontSizePx = fontSizePx;
  outCtx.exPx = fontSizePx * 0.5f; // TODO：M2 接 QFontMetricsF::xHeight()。
  outCtx.chPx = fontSizePx * 0.5f; // TODO：M2 接字符 '0' 的 advance。
  outCtx.lineHeightPx = fontSizePx * 1.2f;
  ResolveContext& ctx = outCtx;

  // ---- 阶段 3：font 属性（真实 font metrics 仍是 M2） --------------------
  if (const auto* ff = declared<lxb_css_property_font_family_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_FAMILY)) {
    if (ff->first != nullptr) {
      const lxb_css_property_family_name_t* name = ff->first;
      if (name->generic) {
        builder.Font().family = genericFontFamily(name->u.type);
      } else {
        builder.Font().family = heap_.internString(
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
      builder.Font().weight = 400;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      if (fw->type == LXB_CSS_FONT_WEIGHT_BOLD) {
        builder.Font().weight = 700;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT_BOLDER) {
        builder.Font().weight = parent.Font().weight < 600 ? 700 : 900;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT_LIGHTER) {
        builder.Font().weight = parent.Font().weight >= 600 ? 400 : 100;
      } else if (fw->type == LXB_CSS_FONT_WEIGHT__NUMBER) {
        builder.Font().weight = static_cast<int>(fw->number.num);
      } else {
        builder.Font().weight = 400;
      }
      break;
    }
  }
  if (const auto* fs = declared<lxb_css_property_font_style_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_STYLE)) {
    switch (wideFromType(fs->type)) {
    case Wide::Initial:
      builder.Font().style = LXB_CSS_FONT_STYLE_NORMAL;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.Font().style = fs->type;
      break;
    }
  }
  if (const auto* fst = declared<lxb_css_property_font_stretch_t>(
          cascaded,
          LXB_CSS_PROPERTY_FONT_STRETCH)) {
    switch (wideFromType(fst->type)) {
    case Wide::Initial:
      builder.Font().stretchPercent = 100.0f;
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      if (fst->type == LXB_CSS_FONT_STRETCH__PERCENTAGE) {
        builder.Font().stretchPercent = static_cast<float>(fst->percentage.num);
      } else {
        builder.Font().stretchPercent = fontStretchPercent(fst->type);
      }
      break;
    }
  }

  // ---- 阶段 4：line-height（lh 单位；M1 简化实现） -------------------------
  if (const auto* lh = declared<lxb_css_property_line_height_t>(
          cascaded,
          LXB_CSS_PROPERTY_LINE_HEIGHT)) {
    LineHeight value = parent.InheritedData().lineHeight;
    switch (wideFromType(lh->type)) {
    case Wide::Initial:
      value = LineHeight{};
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      value = parent.InheritedData().lineHeight;
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
    builder.InheritedData().lineHeight = value;
  }

  // ---- 阶段 5：box / sizing 枚举 ------------------------------------------
  if (const auto* d =
          declared<lxb_css_property_display_t>(cascaded, LXB_CSS_PROPERTY_DISPLAY)) {
    switch (wideFromType(d->a)) {
    case Wide::Initial:
    case Wide::Unset:
      builder.SetDisplay(LXB_CSS_DISPLAY_INLINE);
      break;
    case Wide::Inherit:
      builder.SetDisplay(parent.Display());
      break;
    case Wide::Revert:
      builder.SetDisplay(LXB_CSS_DISPLAY_INLINE);
      break;
    case Wide::None:
      builder.SetDisplay(d->a);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_position_t>(
          cascaded,
          LXB_CSS_PROPERTY_POSITION)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.SetPosition(parent.Position());
    } else if (wideFromType(d->type) == Wide::None) {
      builder.SetPosition(d->type);
    } // initial/unset/revert -> Static，默认值已经是 static。
  }
  if (const auto* d = declared<lxb_css_property_box_sizing_t>(
          cascaded,
          LXB_CSS_PROPERTY_BOX_SIZING)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.BoxData().boxSizing = parent.BoxData().boxSizing;
    } else if (wideFromType(d->type) == Wide::None) {
      builder.BoxData().boxSizing = d->type;
    }
  }
  if (const auto* d =
          declared<lxb_css_property_float_t>(cascaded, LXB_CSS_PROPERTY_FLOAT)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.VisualData().floating = parent.VisualData().floating;
    } else if (wideFromType(d->type) == Wide::None) {
      builder.VisualData().floating = d->type;
    }
  }
  if (const auto* d =
          declared<lxb_css_property_clear_t>(cascaded, LXB_CSS_PROPERTY_CLEAR)) {
    if (wideFromType(d->type) == Wide::Inherit) {
      builder.SetClear(parent.Clear());
    } else if (wideFromType(d->type) == Wide::Initial ||
               wideFromType(d->type) == Wide::Unset ||
               wideFromType(d->type) == Wide::Revert) {
      builder.SetClear(LXB_CSS_CLEAR_NONE);
    } else {
      builder.SetClear(d->type);
    }
  }
  if (const auto* z =
          declared<lxb_css_property_z_index_t>(cascaded, LXB_CSS_PROPERTY_Z_INDEX)) {
    switch (wideFromType(z->type)) {
    case Wide::Inherit:
      builder.BoxData().zIndex = parent.BoxData().zIndex;
      builder.BoxData().zIndexAuto = parent.BoxData().zIndexAuto;
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.BoxData().zIndex = 0;
      builder.BoxData().zIndexAuto = true;
      break;
    case Wide::None:
      if (z->type == LXB_CSS_Z_INDEX__INTEGER) {
        builder.BoxData().zIndex = static_cast<int>(z->integer.num);
        builder.BoxData().zIndexAuto = false;
      } else {
        builder.BoxData().zIndex = 0;
        builder.BoxData().zIndexAuto = true;
      }
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_visibility_t>(
          cascaded,
          LXB_CSS_PROPERTY_VISIBILITY)) {
    switch (wideFromType(d->type)) {
    case Wide::Initial:
      builder.SetVisibility(LXB_CSS_VISIBILITY_VISIBLE);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.SetVisibility(d->type);
      break;
    }
  }

  // ---- Box 尺寸（length-percentage；非继承属性） ----------------------------
  // 易错点：非继承属性遇到 inherit 时必须复制父元素 computed value，
  // 所以每次调用都显式传入 parentVal。
  bool present = false;
  auto setBoxLP = [&](uint16_t id, LengthValue initial, LengthValue parentVal, LengthValue& slot) {
    LengthValue v = resolveLP(cascaded, id, ctx, initial, parentVal, false, &present);
    if (present) slot = v;
  };
  setBoxLP(LXB_CSS_PROPERTY_WIDTH, LengthValue::makeAuto(),
           parent.BoxData().width, builder.BoxData().width);
  setBoxLP(LXB_CSS_PROPERTY_HEIGHT, LengthValue::makeAuto(),
           parent.BoxData().height, builder.BoxData().height);
  setBoxLP(LXB_CSS_PROPERTY_MIN_WIDTH, LengthValue::makePx(0),
           parent.BoxData().minWidth, builder.BoxData().minWidth);
  setBoxLP(LXB_CSS_PROPERTY_MIN_HEIGHT, LengthValue::makePx(0),
           parent.BoxData().minHeight, builder.BoxData().minHeight);
  setBoxLP(LXB_CSS_PROPERTY_MAX_WIDTH, LengthValue::makeNone(),
           parent.BoxData().maxWidth, builder.BoxData().maxWidth);
  setBoxLP(LXB_CSS_PROPERTY_MAX_HEIGHT, LengthValue::makeNone(),
           parent.BoxData().maxHeight, builder.BoxData().maxHeight);

  // ---- Surround：margin / padding / inset（TRBL 物理边） -------------------
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
    LengthValue v = resolveLP(cascaded, m.id, ctx, LengthValue::makePx(0), parent.BoxData().margin[m.side], false, &present);
    if (present) builder.BoxData().margin[m.side] = v;
  }
  for (const auto& p : paddings) {
    LengthValue v = resolveLP(cascaded, p.id, ctx, LengthValue::makePx(0), parent.BoxData().padding[p.side], false, &present);
    if (present) builder.BoxData().padding[p.side] = v;
  }
  for (const auto& in : insets) {
    LengthValue v = resolveLP(cascaded, in.id, ctx, LengthValue::makeAuto(), parent.SurroundData().inset[in.side], false, &present);
    if (present) builder.SurroundData().inset[in.side] = v;
  }

  // ---- 继承 text 细节属性 -------------------------------------------------
  if (const auto* ta = declared<lxb_css_property_text_align_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_ALIGN)) {
    switch (wideFromType(ta->type)) {
    case Wide::Initial:
      builder.SetTextAlign(LXB_CSS_TEXT_ALIGN_START);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.SetTextAlign(ta->type);
      break;
    }
  }
  if (const auto* ta = declared<lxb_css_property_text_align_all_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_ALIGN_ALL)) {
    switch (wideFromType(ta->type)) {
    case Wide::Initial:
      builder.SetTextAlign(LXB_CSS_TEXT_ALIGN_START);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.SetTextAlign(ta->type);
      break;
    }
  }
  if (const auto* ws = declared<lxb_css_property_white_space_t>(
          cascaded,
          LXB_CSS_PROPERTY_WHITE_SPACE)) {
    switch (wideFromType(ws->type)) {
    case Wide::Initial:
      builder.SetWhiteSpace(LXB_CSS_WHITE_SPACE_NORMAL);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.SetWhiteSpace(ws->type);
      break;
    }
  }
  if (const auto* ti = declared<lxb_css_property_text_indent_t>(
          cascaded,
          LXB_CSS_PROPERTY_TEXT_INDENT)) {
    switch (wideFromType(ti->type)) {
    case Wide::Initial:
      builder.RareInheritedData().textIndent = LengthValue::makePx(0);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.RareInheritedData().textIndent =
          resolveLengthPercentage(ti->length, ctx);
      break;
    }
  }
  if (const auto* ls = declared<lxb_css_property_letter_spacing_t>(
          cascaded,
          LXB_CSS_PROPERTY_LETTER_SPACING)) {
    switch (wideFromType(ls->type)) {
    case Wide::Initial:
      builder.InheritedData().letterSpacing = LengthValue::makeAuto();
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.InheritedData().letterSpacing =
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
      builder.InheritedData().wordSpacing = LengthValue::makePx(0);
      break;
    case Wide::Inherit:
    case Wide::Unset:
    case Wide::Revert:
      break; // 继承：保留 inheritFrom() 已共享的父值。
    case Wide::None:
      builder.InheritedData().wordSpacing =
          ws->type == LXB_CSS_WORD_SPACING__LENGTH
              ? LengthValue::makePx(resolveLengthPx(ws->length, ctx))
              : LengthValue::makePx(0);
      break;
    }
  }

  // ---- Border 边 ----------------------------------------------------------
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
    resolveBorderEdge(cascaded, b.id, ctx, currentColor,
                      parent.BoxData().borderWidth[b.side],
                      parent.BoxData().borderStyle[b.side],
                      parent.SurroundData().borderColor[b.side],
                      builder.BoxData().borderWidth[b.side],
                      builder.BoxData().borderStyle[b.side],
                      builder.SurroundData().borderColor[b.side], &present);
    if (present) {
      continue;
    }
  }
  for (const auto& c : borderColors) {
    resolveBorderColor(cascaded, c.id, currentColor,
                       parent.SurroundData().borderColor[c.side],
                       builder.SurroundData().borderColor[c.side]);
  }

  // ---- background / svg / top-level overflow -----------------------------
  if (const auto* c = declared<lxb_css_property_background_color_t>(
          cascaded,
          LXB_CSS_PROPERTY_BACKGROUND_COLOR)) {
    Color out;
    switch (wideFromType(c->type)) {
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.BackgroundData().backgroundColor = Color::transparent();
      break;
    case Wide::Inherit:
      builder.BackgroundData().backgroundColor =
          parent.BackgroundData().backgroundColor;
      break;
    case Wide::None:
      if (convColor(*c, currentColor, out)) {
        builder.BackgroundData().backgroundColor = out;
      }
      break;
    }
  }
  if (const auto* o = declared<lxb_css_property_opacity_t>(
          cascaded,
          LXB_CSS_PROPERTY_OPACITY)) {
    if (o->type == LXB_CSS_VALUE__NUMBER) {
      builder.SvgData().opacity = static_cast<float>(o->u.number.num);
    } else if (o->type == LXB_CSS_VALUE__PERCENTAGE) {
      builder.SvgData().opacity =
          static_cast<float>(o->u.percentage.num) / 100.0f;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_x_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_X)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.SetOverflowX(parent.OverflowX());
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.SetOverflowX(LXB_CSS_OVERFLOW_X_VISIBLE);
      break;
    case Wide::None:
      builder.SetOverflowX(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_y_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_Y)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.SetOverflowY(parent.OverflowY());
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.SetOverflowY(LXB_CSS_OVERFLOW_Y_VISIBLE);
      break;
    case Wide::None:
      builder.SetOverflowY(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_inline_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_INLINE)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.SetOverflowX(parent.OverflowX());
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.SetOverflowX(LXB_CSS_OVERFLOW_X_VISIBLE);
      break;
    case Wide::None:
      builder.SetOverflowX(d->type);
      break;
    }
  }
  if (const auto* d = declared<lxb_css_property_overflow_block_t>(
          cascaded,
          LXB_CSS_PROPERTY_OVERFLOW_BLOCK)) {
    switch (wideFromType(d->type)) {
    case Wide::Inherit:
      builder.SetOverflowY(parent.OverflowY());
      break;
    case Wide::Initial:
    case Wide::Unset:
    case Wide::Revert:
      builder.SetOverflowY(LXB_CSS_OVERFLOW_Y_VISIBLE);
      break;
    case Wide::None:
      builder.SetOverflowY(d->type);
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
  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    releaseStyle(style);
    return;
  }
  releaseStyle(state->style);
  state->style = style;
  state->context = outCtx;
  clearDirty(node);

  for (lxb_dom_node_t* child = lxb_dom_interface_node(element)->first_child;
       child != nullptr;
       child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      resolveSubtree(lxb_dom_interface_element(child), *style, outCtx);
    }
  }
}

const ComputedStyle*
StyleResolver::resolveElementIncremental(lxb_dom_element_t* element) {
  if (element == nullptr) {
    return nullptr;
  }

  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  const ComputedStyle* parentStyle = &initialStyle_;
  const ResolveContext* parentCtx = &rootCtx_;

  lxb_dom_node_t* parent = node->parent;
  while (parent != nullptr && parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    parent = parent->parent;
  }
  if (parent != nullptr) {
    if (const ComputedStyle* resolvedParent = styleFor(parent)) {
      parentStyle = resolvedParent;
      if (const ResolveContext* resolvedParentCtx = contextFor(parent)) {
        parentCtx = resolvedParentCtx;
      }
    }
  }

  ResolveContext outCtx;
  ComputedStyle* style = resolveElement(element, *parentStyle, *parentCtx, outCtx);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    releaseStyle(style);
    return nullptr;
  }
  releaseStyle(state->style);
  state->style = style;
  state->context = outCtx;
  clearDirty(node);
  return style;
}

void StyleResolver::releaseStyle(ComputedStyle* style) noexcept {
  if (style == nullptr) {
    return;
  }
  style->ReleaseArenaRefs(heap_);
  heap_.freePod(style);
}

void StyleResolver::resolveSubtreeIncremental(lxb_dom_element_t* element) {
  const ComputedStyle* style = resolveElementIncremental(element);
  if (style == nullptr) {
    return;
  }

  for (lxb_dom_node_t* child = lxb_dom_interface_node(element)->first_child;
       child != nullptr;
       child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      resolveSubtreeIncremental(lxb_dom_interface_element(child));
    }
  }
}

void StyleResolver::resolvePendingSubtreeIncremental(lxb_dom_node_t* root) {
  resolvePendingSubtree(root, false);
}

void StyleResolver::resolvePendingSubtree(lxb_dom_node_t* root, bool force) {
  if (root == nullptr) {
    return;
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    const bool missing = styleFor(root) == nullptr;
    const bool dirty = isDirty(root);
    force = force || dirty;
    if (missing || force) {
      resolveElementIncremental(lxb_dom_interface_element(root));
    } else if (!hasDirtyDescendant(root)) {
      return;
    }
  }

  for (lxb_dom_node_t* child = root->first_child; child != nullptr;
       child = child->next) {
    resolvePendingSubtree(child, force);
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    if (NodeState* state = ensureState(root)) {
      state->flags &= ~kNodeChildDirty;
    }
  }
}

void StyleResolver::markStyleDirty(lxb_dom_element_t* element) {
  if (element == nullptr) {
    return;
  }

  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    return;
  }
  state->flags |= kNodeSelfDirty;
  markChildDirtyAncestors(node);
}

void StyleResolver::markChildrenDirty(lxb_dom_node_t* parent) {
  if (parent == nullptr || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return;
  }

  NodeState* state = ensureState(parent);
  if (state == nullptr) {
    return;
  }
  state->flags |= kNodeChildDirty;
  markChildDirtyAncestors(parent);
}

void StyleResolver::markSubtreeStyleDirty(lxb_dom_node_t* root) {
  if (root == nullptr) {
    return;
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    NodeState* state = ensureState(root);
    if (state == nullptr) {
      return;
    }
    state->flags |= kNodeSubtreeDirty;
    markChildDirtyAncestors(root);
    return;
  }

  for (lxb_dom_node_t* child = root->first_child; child != nullptr;
       child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      markSubtreeStyleDirty(child);
      return;
    }
  }
}

void StyleResolver::resolveDocument(lxb_html_document_t* doc,
                                    const ResolveContext& rootCtx) {
  beginDocument(rootCtx);
  lxb_dom_document_t* dom = lxb_dom_interface_document(doc);
  lxb_dom_element_t* root = lxb_dom_document_element(dom);
  if (root == nullptr) {
    return;
  }
  resolvePendingSubtreeIncremental(lxb_dom_interface_node(root));
}

const ComputedStyle* StyleResolver::styleFor(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state == nullptr ? nullptr : state->style;
}

StyleResolver::NodeState* StyleResolver::ensureState(lxb_dom_node_t* node) {
  if (node == nullptr || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return nullptr;
  }

  if (node->user != nullptr) {
    NodeState* state = static_cast<NodeState*>(node->user);
    if (state->owner == this && state->generation == generation_ &&
        state->node == node) {
      return state;
    }
  }

  void* storage = lexbor_dobject_calloc(nodeStates_);
  if (storage == nullptr) {
    throw std::bad_alloc();
  }

  NodeState* state = new (storage) NodeState();
  state->owner = this;
  state->node = node;
  state->generation = generation_;
  node->user = state;
  return state;
}

const StyleResolver::NodeState*
StyleResolver::stateFor(const lxb_dom_node_t* node) const {
  if (node == nullptr || node->user == nullptr) {
    return nullptr;
  }
  const NodeState* state = static_cast<const NodeState*>(node->user);
  return state->owner == this && state->generation == generation_ &&
                 state->node == node
             ? state
             : nullptr;
}

const ResolveContext*
StyleResolver::contextFor(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state == nullptr || state->style == nullptr ? nullptr : &state->context;
}

bool StyleResolver::isDirty(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state != nullptr &&
         (state->flags & (kNodeSelfDirty | kNodeSubtreeDirty)) != 0;
}

bool StyleResolver::hasDirtyDescendant(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state != nullptr && (state->flags & kNodeChildDirty) != 0;
}

void StyleResolver::clearDirty(lxb_dom_node_t* node) {
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    return;
  }
  state->flags &= ~(kNodeSelfDirty | kNodeSubtreeDirty);
}

void StyleResolver::markChildDirtyAncestors(lxb_dom_node_t* node) {
  for (lxb_dom_node_t* parent = node == nullptr ? nullptr : node->parent;
       parent != nullptr && parent->type == LXB_DOM_NODE_TYPE_ELEMENT;
       parent = parent->parent) {
    NodeState* state = ensureState(parent);
    if ((state->flags & kNodeChildDirty) != 0) {
      break;
    }
    state->flags |= kNodeChildDirty;
  }
}

} // namespace style
