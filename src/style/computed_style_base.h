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
#include <string>

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

  void releaseRef() const {
    if (!isImmortal_ && refCount_ > 0) {
      --refCount_;
    }
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

  ~DataRef() {
    p_->releaseRef();
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
  std::string family = "sans-serif";
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
  FontDesc font{};
  LineHeight lineHeight{};
  LengthValue letterSpacing = LengthValue::makeAuto();
  LengthValue wordSpacing = LengthValue::makePx(0.0f);
  LengthValue textIndent = LengthValue::makePx(0.0f);
  lxb_css_text_align_type_t textAlign = LXB_CSS_TEXT_ALIGN_START;
  lxb_css_white_space_type_t whiteSpace = LXB_CSS_WHITE_SPACE_NORMAL;

  bool operator==(const StyleInheritedData& o) const {
    return color == o.color && font == o.font && lineHeight == o.lineHeight &&
           letterSpacing == o.letterSpacing && wordSpacing == o.wordSpacing &&
           textIndent == o.textIndent && textAlign == o.textAlign &&
           whiteSpace == o.whiteSpace;
  }
  bool operator!=(const StyleInheritedData& o) const {
    return !(*this == o);
  }
};

struct StyleInheritedOtherData : public StyleData {
  lxb_css_direction_type_t direction = LXB_CSS_DIRECTION_LTR;
  lxb_css_writing_mode_type_t writingMode = LXB_CSS_WRITING_MODE_HORIZONTAL_TB;
  lxb_css_visibility_type_t visibility = LXB_CSS_VISIBILITY_VISIBLE;

  bool operator==(const StyleInheritedOtherData& o) const {
    return direction == o.direction && writingMode == o.writingMode &&
           visibility == o.visibility;
  }
  bool operator!=(const StyleInheritedOtherData& o) const {
    return !(*this == o);
  }
};

struct StyleBoxData : public StyleData {
  lxb_css_display_type_t display = LXB_CSS_DISPLAY_INLINE;
  lxb_css_position_type_t position = LXB_CSS_POSITION_STATIC;
  LengthValue width = LengthValue::makeAuto();
  LengthValue height = LengthValue::makeAuto();
  LengthValue minWidth = LengthValue::makePx(0.0f);
  LengthValue minHeight = LengthValue::makePx(0.0f);
  LengthValue maxWidth = LengthValue::makeNone();
  LengthValue maxHeight = LengthValue::makeNone();
  lxb_css_box_sizing_type_t boxSizing = LXB_CSS_BOX_SIZING_CONTENT_BOX;
  lxb_css_float_type_t floatKind = LXB_CSS_FLOAT_NONE;
  lxb_css_clear_type_t clear = LXB_CSS_CLEAR_NONE;
  int zIndex = 0;
  bool zIndexAuto = true;

  bool operator==(const StyleBoxData& o) const {
    return display == o.display && position == o.position && width == o.width &&
           height == o.height && minWidth == o.minWidth &&
           minHeight == o.minHeight && maxWidth == o.maxWidth &&
           maxHeight == o.maxHeight && boxSizing == o.boxSizing &&
           floatKind == o.floatKind && clear == o.clear &&
           zIndex == o.zIndex && zIndexAuto == o.zIndexAuto;
  }
  bool operator!=(const StyleBoxData& o) const {
    return !(*this == o);
  }
};

struct StyleSurroundData : public StyleData {
  LengthValue margin[4] = {LengthValue::makePx(0.0f), LengthValue::makePx(0.0f), LengthValue::makePx(0.0f), LengthValue::makePx(0.0f)};
  LengthValue padding[4] = {LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f),
                            LengthValue::makePx(0.0f)};
  LengthValue inset[4] = {LengthValue::makeAuto(), LengthValue::makeAuto(), LengthValue::makeAuto(), LengthValue::makeAuto()};
  BorderEdge border[4] = {};

  bool operator==(const StyleSurroundData& o) const {
    for (int i = 0; i < 4; ++i) {
      if (margin[i] != o.margin[i] || padding[i] != o.padding[i] ||
          inset[i] != o.inset[i] || border[i] != o.border[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const StyleSurroundData& o) const {
    return !(*this == o);
  }
};

struct StyleVisualData : public StyleData {
  Color background = Color::transparent();
  float opacity = 1.0f;
  lxb_css_overflow_x_type_t overflowX = LXB_CSS_OVERFLOW_X_VISIBLE;
  lxb_css_overflow_y_type_t overflowY = LXB_CSS_OVERFLOW_Y_VISIBLE;

  bool operator==(const StyleVisualData& o) const {
    return background == o.background && opacity == o.opacity &&
           overflowX == o.overflowX && overflowY == o.overflowY;
  }
  bool operator!=(const StyleVisualData& o) const {
    return !(*this == o);
  }
};

using InheritedText = StyleInheritedData;
using InheritedOther = StyleInheritedOtherData;
using BoxGroup = StyleBoxData;
using SurroundGroup = StyleSurroundData;
using VisualGroup = StyleVisualData;

class ComputedStyleBase {
public:
  ComputedStyleBase() = default;

  const StyleInheritedData& InheritedData() const {
    return inheritedData_.get();
  }
  const StyleInheritedOtherData& InheritedOtherData() const {
    return inheritedOtherData_.get();
  }
  const StyleBoxData& BoxData() const {
    return boxData_.get();
  }
  const StyleSurroundData& SurroundData() const {
    return surroundData_.get();
  }
  const StyleVisualData& VisualData() const {
    return visualData_.get();
  }

  StyleInheritedData& MutableInheritedData(StyleHeap& heap) {
    return inheritedData_.mutate(heap);
  }
  StyleInheritedOtherData& MutableInheritedOtherData(StyleHeap& heap) {
    return inheritedOtherData_.mutate(heap);
  }
  StyleBoxData& MutableBoxData(StyleHeap& heap) {
    return boxData_.mutate(heap);
  }
  StyleSurroundData& MutableSurroundData(StyleHeap& heap) {
    return surroundData_.mutate(heap);
  }
  StyleVisualData& MutableVisualData(StyleHeap& heap) {
    return visualData_.mutate(heap);
  }

  void InheritDataFrom(const ComputedStyleBase& parent) {
    inheritedData_ = parent.inheritedData_;
    inheritedOtherData_ = parent.inheritedOtherData_;
  }

  void ResetNonInheritedData() {
    boxData_ = DataRef<StyleBoxData>{};
    surroundData_ = DataRef<StyleSurroundData>{};
    visualData_ = DataRef<StyleVisualData>{};
  }

  bool InheritedEqual(const ComputedStyleBase& o) const {
    return inheritedData_.equals(o.inheritedData_) &&
           inheritedOtherData_.equals(o.inheritedOtherData_);
  }

  bool NonInheritedEqual(const ComputedStyleBase& o) const {
    return boxData_.equals(o.boxData_) &&
           surroundData_.equals(o.surroundData_) &&
           visualData_.equals(o.visualData_);
  }

  bool InheritedDataShared(const ComputedStyleBase& o) const {
    return inheritedData_.sharesWith(o.inheritedData_) &&
           inheritedOtherData_.sharesWith(o.inheritedOtherData_);
  }

  const DataRef<StyleInheritedData>& inheritedDataRef() const {
    return inheritedData_;
  }
  const DataRef<StyleInheritedOtherData>& inheritedOtherDataRef() const {
    return inheritedOtherData_;
  }
  const DataRef<StyleBoxData>& boxDataRef() const {
    return boxData_;
  }
  const DataRef<StyleSurroundData>& surroundDataRef() const {
    return surroundData_;
  }
  const DataRef<StyleVisualData>& visualDataRef() const {
    return visualData_;
  }

  // 兼容旧 layout 调用；后续迁移到 generated-style 命名后可逐步删除。
  const StyleInheritedData& text() const {
    return InheritedData();
  }
  const StyleInheritedOtherData& inheritedOther() const {
    return InheritedOtherData();
  }
  const StyleBoxData& box() const {
    return BoxData();
  }
  const StyleSurroundData& surround() const {
    return SurroundData();
  }
  const StyleVisualData& visual() const {
    return VisualData();
  }

  StyleInheritedData& mutableText(StyleHeap& heap) {
    return MutableInheritedData(heap);
  }
  StyleInheritedOtherData& mutableInheritedOther(StyleHeap& heap) {
    return MutableInheritedOtherData(heap);
  }
  StyleBoxData& mutableBox(StyleHeap& heap) {
    return MutableBoxData(heap);
  }
  StyleSurroundData& mutableSurround(StyleHeap& heap) {
    return MutableSurroundData(heap);
  }
  StyleVisualData& mutableVisual(StyleHeap& heap) {
    return MutableVisualData(heap);
  }

private:
  DataRef<StyleInheritedData> inheritedData_;
  DataRef<StyleInheritedOtherData> inheritedOtherData_;
  DataRef<StyleBoxData> boxData_;
  DataRef<StyleSurroundData> surroundData_;
  DataRef<StyleVisualData> visualData_;
};

} // namespace style
