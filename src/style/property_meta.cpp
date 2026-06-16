#include "property_meta.h"

#include <array>

#include "lexbor/css/property/const.h"

namespace style {

namespace {

constexpr size_t kCount = LXB_CSS_PROPERTY__LAST_ENTRY; // lexbor 当前 property 表大小。

using Table = std::array<PropertyMeta, kCount>;

// 设置指定 property id 的继承性、存储分组和解析阶段。
inline void set(Table& t, uint16_t id, bool inherited, Group g,
                Phase p = Phase::Normal, bool shorthand = false) {
  if (id < kCount) {
    t[id] = PropertyMeta{id, inherited, g, p, shorthand};
  }
}

Table buildTable() {
  Table t{};

  // 默认每个槽位都是 {id, 非继承, TopLevel, Normal}。
  // 易错点：未知属性不能默认继承，否则新增 lexbor id 可能污染子树。
  for (uint16_t id = 0; id < kCount; ++id) {
    t[id] = PropertyMeta{id, false, Group::TopLevel, Phase::Normal, false};
  }

  // --- 阶段 0：writing context（逻辑边转物理边的输入） ----------------------
  set(t, LXB_CSS_PROPERTY_WRITING_MODE, true, Group::TopLevel, Phase::WritingContext);
  set(t, LXB_CSS_PROPERTY_DIRECTION, true, Group::TopLevel, Phase::WritingContext);

  // --- 阶段 1：color（currentColor 来源） ----------------------------------
  set(t, LXB_CSS_PROPERTY_COLOR, true, Group::Inherited, Phase::ColorContext);

  // --- 阶段 2：font-size（em/ex/ch 基准） ----------------------------------
  set(t, LXB_CSS_PROPERTY_FONT_SIZE, true, Group::TopLevel, Phase::FontSize);

  // --- 阶段 3：font 属性（后续生成 QFont/metrics） --------------------------
  set(t, LXB_CSS_PROPERTY_FONT_FAMILY, true, Group::TopLevel, Phase::FontProps);
  set(t, LXB_CSS_PROPERTY_FONT_STYLE, true, Group::TopLevel, Phase::FontProps);
  set(t, LXB_CSS_PROPERTY_FONT_WEIGHT, true, Group::TopLevel, Phase::FontProps);
  set(t, LXB_CSS_PROPERTY_FONT_STRETCH, true, Group::TopLevel, Phase::FontProps);

  // --- 阶段 4：line-height（lh 单位基准） ----------------------------------
  set(t, LXB_CSS_PROPERTY_LINE_HEIGHT, true, Group::Inherited, Phase::LineHeight);

  // --- 其它继承 text 属性（Normal 阶段） -----------------------------------
  set(t, LXB_CSS_PROPERTY_LETTER_SPACING, true, Group::Inherited);
  set(t, LXB_CSS_PROPERTY_WORD_SPACING, true, Group::Inherited);
  set(t, LXB_CSS_PROPERTY_TEXT_ALIGN, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_ALIGN_ALL, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_ALIGN_LAST, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_INDENT, true, Group::RareInherited);
  set(t, LXB_CSS_PROPERTY_TEXT_TRANSFORM, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_JUSTIFY, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_WHITE_SPACE, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_WORD_BREAK, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_WORD_WRAP, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_OVERFLOW_WRAP, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_LINE_BREAK, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_HYPHENS, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TAB_SIZE, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_HANGING_PUNCTUATION, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_COMBINE_UPRIGHT, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_ORIENTATION, true, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_DOMINANT_BASELINE, true, Group::TopLevel);

  // --- 继承但非 text 的属性 ------------------------------------------------
  set(t, LXB_CSS_PROPERTY_VISIBILITY, true, Group::TopLevel);

  // --- Box（非继承 layout 属性） -------------------------------------------
  set(t, LXB_CSS_PROPERTY_DISPLAY, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_POSITION, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_WIDTH, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_HEIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MIN_WIDTH, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MIN_HEIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MAX_WIDTH, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MAX_HEIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BOX_SIZING, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLOAT, false, Group::Visual);
  set(t, LXB_CSS_PROPERTY_CLEAR, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_FLOAT_DEFER, false, Group::Visual);
  set(t, LXB_CSS_PROPERTY_FLOAT_OFFSET, false, Group::Visual);
  set(t, LXB_CSS_PROPERTY_FLOAT_REFERENCE, false, Group::Visual);
  set(t, LXB_CSS_PROPERTY_ORDER, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLEX_BASIS, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLEX_DIRECTION, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLEX_GROW, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLEX_SHRINK, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_FLEX_WRAP, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_JUSTIFY_CONTENT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_ALIGN_CONTENT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_ALIGN_ITEMS, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_ALIGN_SELF, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_Z_INDEX, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_VERTICAL_ALIGN, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BASELINE_SOURCE, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BASELINE_SHIFT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_ALIGNMENT_BASELINE, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_WRAP_FLOW, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_WRAP_THROUGH, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_UNICODE_BIDI, false, Group::Box);

  // --- Box：margin/padding、border width/style、sizing ---------------------
  set(t, LXB_CSS_PROPERTY_MARGIN_TOP, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MARGIN_RIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MARGIN_BOTTOM, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_MARGIN_LEFT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_PADDING_TOP, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_PADDING_RIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_PADDING_BOTTOM, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_PADDING_LEFT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BORDER_TOP, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BORDER_RIGHT, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BORDER_BOTTOM, false, Group::Box);
  set(t, LXB_CSS_PROPERTY_BORDER_LEFT, false, Group::Box);

  // --- Surround：inset、border color --------------------------------------
  set(t, LXB_CSS_PROPERTY_BORDER_TOP_COLOR, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_BORDER_LEFT_COLOR, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_TOP, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_RIGHT, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_BOTTOM, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_LEFT, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_INSET_BLOCK_START, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_INSET_BLOCK_END, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_INSET_INLINE_START, false, Group::Surround);
  set(t, LXB_CSS_PROPERTY_INSET_INLINE_END, false, Group::Surround);

  // --- Background / SVG / top-level ---------------------------------------
  set(t, LXB_CSS_PROPERTY_BACKGROUND_COLOR, false, Group::Background);
  set(t, LXB_CSS_PROPERTY_OPACITY, false, Group::Svg);
  set(t, LXB_CSS_PROPERTY_OVERFLOW_X, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_OVERFLOW_Y, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_OVERFLOW_BLOCK, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_OVERFLOW_INLINE, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_OVERFLOW, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_DECORATION_LINE, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_DECORATION_STYLE, false, Group::TopLevel);
  set(t, LXB_CSS_PROPERTY_TEXT_DECORATION_COLOR, false, Group::TopLevel);

  // --- Shorthand：lexbor 已展开成 longhand，resolver 跳过 -------------------
  set(t, LXB_CSS_PROPERTY_MARGIN, false, Group::Shorthand, Phase::Normal, true);
  set(t, LXB_CSS_PROPERTY_PADDING, false, Group::Shorthand, Phase::Normal, true);
  set(t, LXB_CSS_PROPERTY_BORDER, false, Group::Shorthand, Phase::Normal, true);
  set(t, LXB_CSS_PROPERTY_FLEX, false, Group::Shorthand, Phase::Normal, true);
  set(t, LXB_CSS_PROPERTY_FLEX_FLOW, false, Group::Shorthand, Phase::Normal, true);
  set(t, LXB_CSS_PROPERTY_TEXT_DECORATION, false, Group::Shorthand, Phase::Normal, true);

  return t;
}

const Table& table() {
  static const Table t = buildTable();
  return t;
}

} // namespace

size_t propertyCount() {
  return kCount;
}

const PropertyMeta& propertyMeta(uint16_t id) {
  static const PropertyMeta fallback{};
  if (id >= kCount) {
    return fallback;
  }
  return table()[id];
}

bool isColorProperty(uint16_t id) {
  switch (id) {
  case LXB_CSS_PROPERTY_COLOR:
  case LXB_CSS_PROPERTY_BACKGROUND_COLOR:
  case LXB_CSS_PROPERTY_BORDER_TOP_COLOR:
  case LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR:
  case LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
  case LXB_CSS_PROPERTY_BORDER_LEFT_COLOR:
  case LXB_CSS_PROPERTY_TEXT_DECORATION_COLOR:
    return true;
  default:
    return false;
  }
}

bool isLengthProperty(uint16_t id) {
  switch (id) {
  case LXB_CSS_PROPERTY_WIDTH:
  case LXB_CSS_PROPERTY_HEIGHT:
  case LXB_CSS_PROPERTY_MIN_WIDTH:
  case LXB_CSS_PROPERTY_MIN_HEIGHT:
  case LXB_CSS_PROPERTY_MAX_WIDTH:
  case LXB_CSS_PROPERTY_MAX_HEIGHT:
  case LXB_CSS_PROPERTY_MARGIN_TOP:
  case LXB_CSS_PROPERTY_MARGIN_RIGHT:
  case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
  case LXB_CSS_PROPERTY_MARGIN_LEFT:
  case LXB_CSS_PROPERTY_PADDING_TOP:
  case LXB_CSS_PROPERTY_PADDING_RIGHT:
  case LXB_CSS_PROPERTY_PADDING_BOTTOM:
  case LXB_CSS_PROPERTY_PADDING_LEFT:
  case LXB_CSS_PROPERTY_TOP:
  case LXB_CSS_PROPERTY_RIGHT:
  case LXB_CSS_PROPERTY_BOTTOM:
  case LXB_CSS_PROPERTY_LEFT:
  case LXB_CSS_PROPERTY_INSET_BLOCK_START:
  case LXB_CSS_PROPERTY_INSET_BLOCK_END:
  case LXB_CSS_PROPERTY_INSET_INLINE_START:
  case LXB_CSS_PROPERTY_INSET_INLINE_END:
  case LXB_CSS_PROPERTY_FONT_SIZE:
  case LXB_CSS_PROPERTY_LINE_HEIGHT:
  case LXB_CSS_PROPERTY_TEXT_INDENT:
  case LXB_CSS_PROPERTY_LETTER_SPACING:
  case LXB_CSS_PROPERTY_WORD_SPACING:
  case LXB_CSS_PROPERTY_FLEX_BASIS:
    return true;
  default:
    return false;
  }
}

} // namespace style
