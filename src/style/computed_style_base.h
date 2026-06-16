#pragma once

// ComputedStyleBase 是 generated-style 风格的底层存储层。
//
// Blink 用 ComputedStyleBase 放密集字段、生成访问器、resetter，以及继承/非继承
// 分组的相等性判断。本项目暂时手写这些字段，但边界保持一致：
// ComputedStyleBase 只存 computed value；ComputedStyle 再叠加更高层样式行为。
//
// 易错点：这里的 DataRef 是轻量 COW，不是 shared_ptr。样式数据对象由 StyleHeap
// arena 批量释放，引用计数只用于判断是否需要 clone，不负责独立 delete。

#include <cstdint>
#include <type_traits>

#include "style_heap.h"
#include "value_types.h"

namespace style {

class StyleData {
public:
  StyleData() = default;
  StyleData(const StyleData&) {
  }
  StyleData& operator=(const StyleData&) {
    return *this;
  }

  void addRef() const {
    if (!isImmortal_) {
      ++refCount_;
    }
  }

  uint32_t releaseRef() const {
    if (!isImmortal_ && refCount_ > 0) {
      --refCount_;
    }
    return refCount_;
  }

  bool hasOneRef() const {
    return !isImmortal_ && refCount_ == 1;
  }
  bool isImmortal() const {
    return isImmortal_;
  }
  void makeImmortal() const {
    isImmortal_ = true;
  }

private:
  mutable uint32_t refCount_ = 0;
  mutable bool isImmortal_ = false;
};

template <class T>
class DataRef {
public:
  DataRef()
      : p_(defaultInstance()) {
    p_->addRef();
  }

  explicit DataRef(const T* p)
      : p_(p == nullptr ? defaultInstance() : p) {
    p_->addRef();
  }

  DataRef(const DataRef& o)
      : p_(o.p_) {
    p_->addRef();
  }

  DataRef(DataRef&& o) noexcept
      : p_(o.p_) {
    o.p_ = defaultInstance();
  }

  DataRef& operator=(const DataRef& o) {
    if (p_ == o.p_) {
      return *this;
    }

    o.p_->addRef();
    p_->releaseRef();
    p_ = o.p_;
    return *this;
  }

  DataRef& operator=(DataRef&& o) noexcept {
    if (p_ == o.p_) {
      return *this;
    }

    p_->releaseRef();
    p_ = o.p_;
    o.p_ = defaultInstance();
    return *this;
  }

  const T& get() const {
    return *p_;
  }
  const T* operator->() const {
    return p_;
  }
  const T& operator*() const {
    return *p_;
  }

  T& mutate(StyleHeap& heap) {
    if (!p_->hasOneRef()) {
      const T* old = p_;
      old->releaseRef();

      T* clone = heap.createData<T>(*old);
      clone->addRef();
      p_ = clone;
    }

    return *const_cast<T*>(p_);
  }

  void release(StyleHeap& heap) noexcept {
    const T* old = p_;
    p_ = defaultInstance();
    if (!old->isImmortal() && old->releaseRef() == 0) {
      heap.freePod(const_cast<T*>(old));
    }
  }

  bool sharesWith(const DataRef& o) const {
    return p_ == o.p_;
  }
  bool equals(const DataRef& o) const {
    return p_ == o.p_ || *p_ == *o.p_;
  }
  const T* ptr() const {
    return p_;
  }

private:
  static const T* defaultInstance() {
    static T instance;
    static const bool initialized = [] {
      instance.makeImmortal();
      return true;
    }();
    (void)initialized;
    return &instance;
  }

  const T* p_;
};

struct LineHeight {
  enum class Kind : uint8_t { Normal,
                              Number,
                              LengthPx } kind = Kind::Normal;
  float value = 0.0f;

  bool operator==(const LineHeight& o) const {
    return kind == o.kind && value == o.value;
  }
  bool operator!=(const LineHeight& o) const {
    return !(*this == o);
  }
};

struct FontDesc {
  const char* family = kFontFamilySansSerif;
  float sizePx = 16.0f;
  lxb_css_font_style_type_t style = LXB_CSS_FONT_STYLE_NORMAL;
  int weight = 400;
  float stretchPercent = 100.0f;

  bool operator==(const FontDesc& o) const {
    return family == o.family && sizePx == o.sizePx && style == o.style &&
           weight == o.weight && stretchPercent == o.stretchPercent;
  }
  bool operator!=(const FontDesc& o) const {
    return !(*this == o);
  }
};

struct StyleInheritedData : public StyleData {
  Color color = Color::black();
  LineHeight lineHeight{};
  LengthValue letterSpacing = LengthValue::makeAuto();
  LengthValue wordSpacing = LengthValue::makePx(0.0f);

  bool operator==(const StyleInheritedData& o) const {
    return color == o.color && lineHeight == o.lineHeight &&
           letterSpacing == o.letterSpacing && wordSpacing == o.wordSpacing;
  }
  bool operator!=(const StyleInheritedData& o) const {
    return !(*this == o);
  }
};

struct StyleRareInheritedData : public StyleData {
  LengthValue textIndent = LengthValue::makePx(0.0f);

  bool operator==(const StyleRareInheritedData& o) const {
    return textIndent == o.textIndent;
  }
  bool operator!=(const StyleRareInheritedData& o) const {
    return !(*this == o);
  }
};

struct StyleBoxData : public StyleData {
  LengthValue width = LengthValue::makeAuto();
  LengthValue height = LengthValue::makeAuto();
  LengthValue minWidth = LengthValue::makePx(0.0f);
  LengthValue minHeight = LengthValue::makePx(0.0f);
  LengthValue maxWidth = LengthValue::makeNone();
  LengthValue maxHeight = LengthValue::makeNone();
  lxb_css_box_sizing_type_t boxSizing = LXB_CSS_BOX_SIZING_CONTENT_BOX;
  LengthValue margin[4] = {LengthValue::makePx(0.0f),
                           LengthValue::makePx(0.0f),
                           LengthValue::makePx(0.0f),
                           LengthValue::makePx(0.0f)};
  LengthValue padding[4] = {LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f)};
  LengthValue borderWidth[4] = {LengthValue::makePx(0.0f),
                                LengthValue::makePx(0.0f),
                                LengthValue::makePx(0.0f),
                                LengthValue::makePx(0.0f)};
  lxb_css_border_type_t borderStyle[4] = {LXB_CSS_BORDER_NONE,
                                          LXB_CSS_BORDER_NONE,
                                          LXB_CSS_BORDER_NONE,
                                          LXB_CSS_BORDER_NONE};
  int zIndex = 0;
  bool zIndexAuto = true;

  bool operator==(const StyleBoxData& o) const {
    if (width != o.width || height != o.height || minWidth != o.minWidth ||
        minHeight != o.minHeight || maxWidth != o.maxWidth ||
        maxHeight != o.maxHeight || boxSizing != o.boxSizing ||
        zIndex != o.zIndex || zIndexAuto != o.zIndexAuto) {
      return false;
    }
    for (int i = 0; i < 4; ++i) {
      if (margin[i] != o.margin[i] || padding[i] != o.padding[i] ||
          borderWidth[i] != o.borderWidth[i] ||
          borderStyle[i] != o.borderStyle[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const StyleBoxData& o) const {
    return !(*this == o);
  }
};

struct StyleSurroundData : public StyleData {
  LengthValue inset[4] = {LengthValue::makeAuto(), LengthValue::makeAuto(), LengthValue::makeAuto(), LengthValue::makeAuto()};
  Color borderColor[4] = {Color::black(), Color::black(), Color::black(),
                          Color::black()};

  bool operator==(const StyleSurroundData& o) const {
    for (int i = 0; i < 4; ++i) {
      if (inset[i] != o.inset[i] || borderColor[i] != o.borderColor[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const StyleSurroundData& o) const {
    return !(*this == o);
  }
};

struct StyleBackgroundData : public StyleData {
  Color backgroundColor = Color::transparent();

  bool operator==(const StyleBackgroundData& o) const {
    return backgroundColor == o.backgroundColor;
  }
  bool operator!=(const StyleBackgroundData& o) const {
    return !(*this == o);
  }
};

struct StyleSvgData : public StyleData {
  float opacity = 1.0f;

  bool operator==(const StyleSvgData& o) const {
    return opacity == o.opacity;
  }
  bool operator!=(const StyleSvgData& o) const {
    return !(*this == o);
  }
};

struct StyleVisualData : public StyleData {
  lxb_css_float_type_t floating = LXB_CSS_FLOAT_NONE;

  bool operator==(const StyleVisualData& o) const {
    return floating == o.floating;
  }
  bool operator!=(const StyleVisualData& o) const {
    return !(*this == o);
  }
};

class ComputedStyleBase {
public:
  ComputedStyleBase() = default;

  const StyleInheritedData& InheritedData() const {
    return inheritedData_.get();
  }
  const StyleRareInheritedData& RareInheritedData() const {
    return rareInheritedData_.get();
  }
  const StyleBoxData& BoxData() const {
    return boxData_.get();
  }
  const StyleSurroundData& SurroundData() const {
    return surroundData_.get();
  }
  const StyleBackgroundData& BackgroundData() const {
    return backgroundData_.get();
  }
  const StyleSvgData& SvgData() const {
    return svgData_.get();
  }
  const StyleVisualData& VisualData() const {
    return visualData_.get();
  }

  StyleInheritedData& MutableInheritedData(StyleHeap& heap) {
    return inheritedData_.mutate(heap);
  }
  StyleRareInheritedData& MutableRareInheritedData(StyleHeap& heap) {
    return rareInheritedData_.mutate(heap);
  }
  StyleBoxData& MutableBoxData(StyleHeap& heap) {
    return boxData_.mutate(heap);
  }
  StyleSurroundData& MutableSurroundData(StyleHeap& heap) {
    return surroundData_.mutate(heap);
  }
  StyleBackgroundData& MutableBackgroundData(StyleHeap& heap) {
    return backgroundData_.mutate(heap);
  }
  StyleSvgData& MutableSvgData(StyleHeap& heap) {
    return svgData_.mutate(heap);
  }
  StyleVisualData& MutableVisualData(StyleHeap& heap) {
    return visualData_.mutate(heap);
  }

  void InheritDataFrom(const ComputedStyleBase& parent) {
    inheritedData_ = parent.inheritedData_;
    rareInheritedData_ = parent.rareInheritedData_;
    font_ = parent.font_;
    direction_ = parent.direction_;
    writingMode_ = parent.writingMode_;
    visibility_ = parent.visibility_;
    textAlign_ = parent.textAlign_;
    whiteSpace_ = parent.whiteSpace_;
  }

  void ResetNonInheritedData() {
    boxData_ = DataRef<StyleBoxData>{};
    surroundData_ = DataRef<StyleSurroundData>{};
    backgroundData_ = DataRef<StyleBackgroundData>{};
    svgData_ = DataRef<StyleSvgData>{};
    visualData_ = DataRef<StyleVisualData>{};
    display_ = LXB_CSS_DISPLAY_INLINE;
    position_ = LXB_CSS_POSITION_STATIC;
    clear_ = LXB_CSS_CLEAR_NONE;
    overflowX_ = LXB_CSS_OVERFLOW_X_VISIBLE;
    overflowY_ = LXB_CSS_OVERFLOW_Y_VISIBLE;
  }

  bool InheritedEqual(const ComputedStyleBase& o) const {
    return inheritedData_.equals(o.inheritedData_) &&
           rareInheritedData_.equals(o.rareInheritedData_) &&
           font_ == o.font_ && direction_ == o.direction_ &&
           writingMode_ == o.writingMode_ && visibility_ == o.visibility_ &&
           textAlign_ == o.textAlign_ && whiteSpace_ == o.whiteSpace_;
  }

  bool NonInheritedEqual(const ComputedStyleBase& o) const {
    return boxData_.equals(o.boxData_) &&
           surroundData_.equals(o.surroundData_) &&
           backgroundData_.equals(o.backgroundData_) &&
           svgData_.equals(o.svgData_) && visualData_.equals(o.visualData_) &&
           display_ == o.display_ && position_ == o.position_ &&
           clear_ == o.clear_ && overflowX_ == o.overflowX_ &&
           overflowY_ == o.overflowY_;
  }

  bool InheritedDataShared(const ComputedStyleBase& o) const {
    return inheritedData_.sharesWith(o.inheritedData_) &&
           rareInheritedData_.sharesWith(o.rareInheritedData_);
  }

  void ReleaseDataRefs(StyleHeap& heap) noexcept {
    inheritedData_.release(heap);
    rareInheritedData_.release(heap);
    boxData_.release(heap);
    surroundData_.release(heap);
    backgroundData_.release(heap);
    svgData_.release(heap);
    visualData_.release(heap);
  }

  const DataRef<StyleInheritedData>& inheritedDataRef() const {
    return inheritedData_;
  }
  const DataRef<StyleRareInheritedData>& rareInheritedDataRef() const {
    return rareInheritedData_;
  }
  const DataRef<StyleBoxData>& boxDataRef() const {
    return boxData_;
  }
  const DataRef<StyleSurroundData>& surroundDataRef() const {
    return surroundData_;
  }
  const DataRef<StyleBackgroundData>& backgroundDataRef() const {
    return backgroundData_;
  }
  const DataRef<StyleSvgData>& svgDataRef() const {
    return svgData_;
  }
  const DataRef<StyleVisualData>& visualDataRef() const {
    return visualData_;
  }

  const FontDesc& Font() const {
    return font_;
  }
  FontDesc& MutableFont() {
    return font_;
  }
  lxb_css_display_type_t Display() const {
    return display_;
  }
  void SetDisplay(lxb_css_display_type_t value) {
    display_ = value;
  }
  lxb_css_position_type_t Position() const {
    return position_;
  }
  void SetPosition(lxb_css_position_type_t value) {
    position_ = value;
  }
  lxb_css_clear_type_t Clear() const {
    return clear_;
  }
  void SetClear(lxb_css_clear_type_t value) {
    clear_ = value;
  }
  lxb_css_direction_type_t Direction() const {
    return direction_;
  }
  void SetDirection(lxb_css_direction_type_t value) {
    direction_ = value;
  }
  lxb_css_writing_mode_type_t WritingMode() const {
    return writingMode_;
  }
  void SetWritingMode(lxb_css_writing_mode_type_t value) {
    writingMode_ = value;
  }
  lxb_css_visibility_type_t Visibility() const {
    return visibility_;
  }
  void SetVisibility(lxb_css_visibility_type_t value) {
    visibility_ = value;
  }
  lxb_css_text_align_type_t TextAlign() const {
    return textAlign_;
  }
  void SetTextAlign(lxb_css_text_align_type_t value) {
    textAlign_ = value;
  }
  lxb_css_white_space_type_t WhiteSpace() const {
    return whiteSpace_;
  }
  void SetWhiteSpace(lxb_css_white_space_type_t value) {
    whiteSpace_ = value;
  }
  lxb_css_overflow_x_type_t OverflowX() const {
    return overflowX_;
  }
  void SetOverflowX(lxb_css_overflow_x_type_t value) {
    overflowX_ = value;
  }
  lxb_css_overflow_y_type_t OverflowY() const {
    return overflowY_;
  }
  void SetOverflowY(lxb_css_overflow_y_type_t value) {
    overflowY_ = value;
  }

private:
  DataRef<StyleInheritedData> inheritedData_;
  DataRef<StyleRareInheritedData> rareInheritedData_;
  DataRef<StyleBoxData> boxData_;
  DataRef<StyleSurroundData> surroundData_;
  DataRef<StyleBackgroundData> backgroundData_;
  DataRef<StyleSvgData> svgData_;
  DataRef<StyleVisualData> visualData_;
  FontDesc font_{};
  lxb_css_display_type_t display_ = LXB_CSS_DISPLAY_INLINE;
  lxb_css_position_type_t position_ = LXB_CSS_POSITION_STATIC;
  lxb_css_clear_type_t clear_ = LXB_CSS_CLEAR_NONE;
  lxb_css_direction_type_t direction_ = LXB_CSS_DIRECTION_LTR;
  lxb_css_writing_mode_type_t writingMode_ = LXB_CSS_WRITING_MODE_HORIZONTAL_TB;
  lxb_css_visibility_type_t visibility_ = LXB_CSS_VISIBILITY_VISIBLE;
  lxb_css_text_align_type_t textAlign_ = LXB_CSS_TEXT_ALIGN_START;
  lxb_css_white_space_type_t whiteSpace_ = LXB_CSS_WHITE_SPACE_NORMAL;
  lxb_css_overflow_x_type_t overflowX_ = LXB_CSS_OVERFLOW_X_VISIBLE;
  lxb_css_overflow_y_type_t overflowY_ = LXB_CSS_OVERFLOW_Y_VISIBLE;
};

static_assert(std::is_trivially_destructible_v<DataRef<StyleInheritedData>>);
static_assert(std::is_trivially_destructible_v<StyleInheritedData>);
static_assert(std::is_trivially_destructible_v<StyleRareInheritedData>);
static_assert(std::is_trivially_destructible_v<StyleBoxData>);
static_assert(std::is_trivially_destructible_v<StyleSurroundData>);
static_assert(std::is_trivially_destructible_v<StyleBackgroundData>);
static_assert(std::is_trivially_destructible_v<StyleSvgData>);
static_assert(std::is_trivially_destructible_v<StyleVisualData>);
static_assert(std::is_trivially_destructible_v<ComputedStyleBase>);

} // namespace style
