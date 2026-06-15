#pragma once

// Self-built style engine — computed value types.
//
// These are the *computed value* representations handed to layout. They are
// deliberately independent of lexbor's parse-time typed structs: the resolver
// (resolver.cpp) translates lexbor declared values into these.
//
// Key invariant: computed != used. Percentages, `auto`, and unresolved calc()
// terms are preserved here as undecided; layout resolves them against a
// containing block (the "used value" stage).

#include <cstdint>
#include <cstring>

namespace style {

// Forward decl — full definition in calc.h (filled in milestone M3).
struct CalcExpr;

// ---------------------------------------------------------------------------
// Color: always concrete (RGBA8) after resolution. `currentColor` and color
// keywords are resolved away by the resolver; layout/paint never sees them.
// ---------------------------------------------------------------------------
struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0; // 0 = fully transparent, 255 = opaque

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
// LengthValue: the computed value of a <length-percentage>-typed property
// (width/height/margin/padding/inset/...).
//
// The whole point of carrying a tagged variant — rather than a bare float — is
// that `Percent`/`Auto`/`Calc` are honestly *undecided* at computed time and
// must survive into layout. Px is the only fully-resolved state.
// ---------------------------------------------------------------------------
struct LengthValue {
  enum class Tag : uint8_t {
    Px,      // resolved absolute pixels
    Percent, // percentage (0..100 == 0%..100%), base unknown until layout
    Auto,    // the `auto` keyword
    Calc,    // calc()/clamp()/min()/max() carrying %/relative terms; see `calc`
    None     // the `none` keyword (e.g. max-width:none)
  };

  Tag tag = Tag::Auto;
  float px = 0.0f;                // valid when tag == Px, or as Percent's number
  const CalcExpr* calc = nullptr; // valid when tag == Calc (arena-owned)

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

// ---------------------------------------------------------------------------
// Keyword-valued enums. Values are our own; the resolver maps lexbor's enums
// onto these. Kept compact (uint8) so the group structs pack tightly.
// ---------------------------------------------------------------------------
enum class Display : uint8_t {
  Inline,
  Block,
  InlineBlock,
  Flex,
  InlineFlex,
  ListItem,
  Table,
  TableRow,
  TableCell,
  None
};

enum class Position : uint8_t { Static,
                                Relative,
                                Absolute,
                                Fixed,
                                Sticky };

enum class BoxSizing : uint8_t { ContentBox,
                                 BorderBox };

enum class Float : uint8_t { None,
                             Left,
                             Right };

enum class Clear : uint8_t { None,
                             Left,
                             Right,
                             Both };

enum class Overflow : uint8_t { Visible,
                                Hidden,
                                Scroll,
                                Auto,
                                Clip };

enum class Visibility : uint8_t { Visible,
                                  Hidden,
                                  Collapse };

enum class Direction : uint8_t { Ltr,
                                 Rtl };

enum class WritingMode : uint8_t {
  HorizontalTb,
  VerticalRl,
  VerticalLr
};

enum class TextAlign : uint8_t { Start,
                                 End,
                                 Left,
                                 Right,
                                 Center,
                                 Justify };

enum class WhiteSpace : uint8_t { Normal,
                                  Pre,
                                  Nowrap,
                                  PreWrap,
                                  PreLine };

enum class FontStyle : uint8_t { Normal,
                                 Italic,
                                 Oblique };

enum class BorderStyle : uint8_t {
  None,
  Hidden,
  Dotted,
  Dashed,
  Solid,
  Double,
  Groove,
  Ridge,
  Inset,
  Outset
};

// Physical side indices for TRBL arrays. Logical sides are mapped onto these
// by the resolver using writing-mode + direction.
enum Side : uint8_t { kTop = 0,
                      kRight = 1,
                      kBottom = 2,
                      kLeft = 3 };

struct BorderEdge {
  LengthValue width = LengthValue::makePx(0.0f); // computed; `thin/medium/thick` resolved
  BorderStyle styleKind = BorderStyle::None;
  Color color = Color::black(); // currentColor resolved away

  bool operator==(const BorderEdge& o) const {
    return width == o.width && styleKind == o.styleKind && color == o.color;
  }
  bool operator!=(const BorderEdge& o) const {
    return !(*this == o);
  }
};

} // namespace style
