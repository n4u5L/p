#pragma once

// 自建样式引擎的 computed value 类型。
//
// 这些结构是 resolver 交给 layout 的 computed value 表示。它们有意与 lexbor
// 解析阶段的 typed struct 分离：resolver.cpp 负责把 lexbor declaration 转成这里的
// 运行时结构。
//
// 核心不变量：computed value != used value。百分比、auto、以及后续 calc() 里的
// 未决项必须保留到 layout 阶段，再结合包含块求 used value。
//
// 易错点：不要把这里简化成裸 float。那会丢掉 percent/auto/calc 的状态，
// 后续布局无法判断“未决”和“已解析 px”的区别。

#include <cstdint>
#include <cstring>

#include "lexbor/css/property/const.h"

namespace style {

// 前置声明；完整 calc 表达式计划在 M3 补齐。
struct CalcExpr;

// ---------------------------------------------------------------------------
// Color：解析后总是具体 RGBA8。currentColor 和颜色关键字由 resolver 消解，
// layout/paint 不再看到它们。
// ---------------------------------------------------------------------------
struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0; // 0 表示完全透明，255 表示不透明。

  constexpr Color() = default;
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
      : r(r_),
        g(g_),
        b(b_),
        a(a_) {
  }

  static constexpr Color transparent() {
    return Color(0, 0, 0, 0);
  }
  static constexpr Color black() {
    return Color(0, 0, 0, 255);
  }

  bool operator==(const Color& o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
  }
  bool operator!=(const Color& o) const {
    return !(*this == o);
  }
};

// ---------------------------------------------------------------------------
// LengthValue：<length-percentage> 属性的 computed value，
// 例如 width/height/margin/padding/inset。
//
// 使用 tag 而不是裸 float 的原因是：Percent/Auto/Calc 在 computed 阶段确实还没
// 决定，必须保留到 layout。只有 Px 是完全解析后的状态。
// ---------------------------------------------------------------------------
struct LengthValue {
  enum class Tag : uint8_t {
    Px,      // 已解析的绝对像素。
    Percent, // 百分比；0..100 对应 0%..100%，基准要等 layout。
    Auto,    // auto 关键字。
    Calc,    // calc()/clamp()/min()/max()；可能携带百分比或相对项。
    None     // none 关键字，例如 max-width:none。
  };

  Tag tag = Tag::Auto;
  float px = 0.0f;                // tag == Px 时是 px；tag == Percent 时是百分比数值。
  const CalcExpr* calc = nullptr; // tag == Calc 时有效，由 arena 持有。

  constexpr LengthValue() = default;

  static constexpr LengthValue makePx(float v) {
    LengthValue l;
    l.tag = Tag::Px;
    l.px = v;
    return l;
  }
  static constexpr LengthValue makePercent(float pct) {
    LengthValue l;
    l.tag = Tag::Percent;
    l.px = pct;
    return l;
  }
  static constexpr LengthValue makeAuto() {
    return LengthValue{};
  }
  static constexpr LengthValue makeNone() {
    LengthValue l;
    l.tag = Tag::None;
    return l;
  }
  static constexpr LengthValue makeCalc(const CalcExpr* e) {
    LengthValue l;
    l.tag = Tag::Calc;
    l.calc = e;
    return l;
  }

  bool isAuto() const {
    return tag == Tag::Auto;
  }
  bool isPx() const {
    return tag == Tag::Px;
  }
  bool isPercent() const {
    return tag == Tag::Percent;
  }
  bool isDefinite() const {
    return tag == Tag::Px;
  }

  bool operator==(const LengthValue& o) const {
    return tag == o.tag && px == o.px && calc == o.calc;
  }
  bool operator!=(const LengthValue& o) const {
    return !(*this == o);
  }
};

// 物理边索引，用于 TRBL 数组。逻辑边由 resolver 根据 writing-mode + direction
// 映射到这些物理边。
enum Side : uint8_t { kTop = 0,
                      kRight = 1,
                      kBottom = 2,
                      kLeft = 3 };

struct BorderEdge {
  LengthValue width = LengthValue::makePx(0.0f); // computed；thin/medium/thick 已解析。
  lxb_css_border_type_t styleKind = LXB_CSS_BORDER_NONE;
  Color color = Color::black(); // currentColor 已解析成具体颜色。

  bool operator==(const BorderEdge& o) const {
    return width == o.width && styleKind == o.styleKind && color == o.color;
  }
  bool operator!=(const BorderEdge& o) const {
    return !(*this == o);
  }
};

} // namespace style
