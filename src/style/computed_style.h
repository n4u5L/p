#pragma once

// ComputedStyle 是 resolver 输出给 layout 的样式对象。
//
// 拆分方式参考 Blink：
//   * ComputedStyleBase 持有密集 computed-value 存储和基础访问器。
//   * ComputedStyle 放继承、diff、自定义属性等高层行为。
//   * ComputedStyleBuilder 是 resolver 专用的写接口。

#include <cstring>
#include <type_traits>

#include "computed_style_base.h"

namespace style {

struct CustomPropEntry {
  const char* name = nullptr;
  const char* value = nullptr;
  CustomPropEntry* next = nullptr;
};

struct CustomPropMap {
  void addRef() const {
    ++refCount;
  }

  uint32_t releaseRef() const {
    if (refCount > 0) {
      --refCount;
    }
    return refCount;
  }

  mutable uint32_t refCount = 0;
  CustomPropEntry* head = nullptr;
};

class ComputedStyle : public ComputedStyleBase {
public:
  ComputedStyle() = default;

  void InheritFrom(const ComputedStyle& parent) {
    InheritDataFrom(parent);
    attachCustomProps(parent.customProps_);
    ResetNonInheritedData();
  }

  void inheritFrom(const ComputedStyle& parent) {
    InheritFrom(parent);
  }

  bool inheritedGroupsShareWith(const ComputedStyle& o) const {
    return InheritedDataShared(o);
  }

  const DataRef<StyleInheritedData>& inheritedRef() const {
    return inheritedDataRef();
  }
  const DataRef<StyleRareInheritedData>& rareInheritedRef() const {
    return rareInheritedDataRef();
  }
  const DataRef<StyleBoxData>& boxRef() const {
    return boxDataRef();
  }
  const DataRef<StyleSurroundData>& surroundRef() const {
    return surroundDataRef();
  }
  const DataRef<StyleBackgroundData>& backgroundRef() const {
    return backgroundDataRef();
  }
  const DataRef<StyleSvgData>& svgRef() const {
    return svgDataRef();
  }
  const DataRef<StyleVisualData>& visualRef() const {
    return visualDataRef();
  }

  const CustomPropMap* customProps() const {
    return customProps_;
  }

  void setCustomPropsForArena(const CustomPropMap* m, StyleHeap& heap) {
    ReleaseCustomProps(heap);
    attachCustomProps(m);
  }

  void ReleaseArenaRefs(StyleHeap& heap) noexcept {
    ReleaseDataRefs(heap);
    ReleaseCustomProps(heap);
  }

private:
  void attachCustomProps(const CustomPropMap* m) {
    customProps_ = m;
    if (customProps_ != nullptr) {
      customProps_->addRef();
    }
  }

  void ReleaseCustomProps(StyleHeap& heap) noexcept {
    auto* props = const_cast<CustomPropMap*>(customProps_);
    customProps_ = nullptr;
    if (props == nullptr || props->releaseRef() != 0) {
      return;
    }

    CustomPropEntry* entry = props->head;
    while (entry != nullptr) {
      CustomPropEntry* next = entry->next;
      heap.freePod(entry);
      entry = next;
    }
    heap.freePod(props);
  }

  const CustomPropMap* customProps_ = nullptr;
};

class ComputedStyleBuilder {
public:
  ComputedStyleBuilder(ComputedStyle& style, StyleHeap& heap)
      : style_(style),
        heap_(heap) {
  }

  StyleInheritedData& InheritedData() {
    return style_.MutableInheritedData(heap_);
  }
  StyleRareInheritedData& RareInheritedData() {
    return style_.MutableRareInheritedData(heap_);
  }
  StyleBoxData& BoxData() {
    return style_.MutableBoxData(heap_);
  }
  StyleSurroundData& SurroundData() {
    return style_.MutableSurroundData(heap_);
  }
  StyleBackgroundData& BackgroundData() {
    return style_.MutableBackgroundData(heap_);
  }
  StyleSvgData& SvgData() {
    return style_.MutableSvgData(heap_);
  }
  StyleVisualData& VisualData() {
    return style_.MutableVisualData(heap_);
  }

  FontDesc& Font() {
    return style_.MutableFont();
  }
  void SetDisplay(lxb_css_display_type_t value) {
    style_.SetDisplay(value);
  }
  void SetPosition(lxb_css_position_type_t value) {
    style_.SetPosition(value);
  }
  void SetClear(lxb_css_clear_type_t value) {
    style_.SetClear(value);
  }
  void SetDirection(lxb_css_direction_type_t value) {
    style_.SetDirection(value);
  }
  void SetWritingMode(lxb_css_writing_mode_type_t value) {
    style_.SetWritingMode(value);
  }
  void SetVisibility(lxb_css_visibility_type_t value) {
    style_.SetVisibility(value);
  }
  void SetTextAlign(lxb_css_text_align_type_t value) {
    style_.SetTextAlign(value);
  }
  void SetWhiteSpace(lxb_css_white_space_type_t value) {
    style_.SetWhiteSpace(value);
  }
  void SetOverflowX(lxb_css_overflow_x_type_t value) {
    style_.SetOverflowX(value);
  }
  void SetOverflowY(lxb_css_overflow_y_type_t value) {
    style_.SetOverflowY(value);
  }

  const CustomPropMap* CustomProps() const {
    return style_.customProps();
  }

  void SetCustomProps(const CustomPropMap& m) {
    CustomPropMap* copy = heap_.create<CustomPropMap>();
    CustomPropEntry** tail = &copy->head;
    for (CustomPropEntry* entry = m.head; entry != nullptr; entry = entry->next) {
      CustomPropEntry* newEntry = heap_.create<CustomPropEntry>(*entry);
      newEntry->next = nullptr;
      *tail = newEntry;
      tail = &newEntry->next;
    }
    style_.setCustomPropsForArena(copy, heap_);
  }

  CustomPropMap& MutableCustomProps() {
    const CustomPropMap* current = style_.customProps();
    CustomPropMap* copy = heap_.create<CustomPropMap>();
    if (current != nullptr) {
      CustomPropEntry** tail = &copy->head;
      for (CustomPropEntry* entry = current->head; entry != nullptr;
           entry = entry->next) {
        CustomPropEntry* newEntry = heap_.create<CustomPropEntry>(*entry);
        newEntry->next = nullptr;
        *tail = newEntry;
        tail = &newEntry->next;
      }
    }
    style_.setCustomPropsForArena(copy, heap_);
    return *copy;
  }

  void SetCustomProp(const char* name, size_t nameLen, const char* value,
                     size_t valueLen) {
    CustomPropMap& props = MutableCustomProps();
    const char* internedName = heap_.internString(name, nameLen);
    const char* internedValue = heap_.internString(value, valueLen);

    for (CustomPropEntry* entry = props.head; entry != nullptr;
         entry = entry->next) {
      if (entry->name == internedName) {
        entry->value = internedValue;
        return;
      }
    }

    CustomPropEntry* entry = heap_.create<CustomPropEntry>();
    entry->name = internedName;
    entry->value = internedValue;
    entry->next = props.head;
    props.head = entry;
  }

  void SetCustomProp(const char* name, const char* value) {
    SetCustomProp(name, std::strlen(name), value, std::strlen(value));
  }

  const CustomPropMap* customProps() const {
    return CustomProps();
  }
  void setCustomProps(const CustomPropMap& m) {
    SetCustomProps(m);
  }
  CustomPropMap& mutableCustomProps() {
    return MutableCustomProps();
  }

private:
  ComputedStyle& style_;
  StyleHeap& heap_;
};

static_assert(std::is_trivially_destructible_v<CustomPropEntry>);
static_assert(std::is_trivially_destructible_v<CustomPropMap>);
static_assert(std::is_trivially_destructible_v<ComputedStyle>);

} // namespace style
