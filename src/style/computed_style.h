#pragma once

// ComputedStyle is the resolver output consumed by layout.
//
// The split mirrors Blink's architecture:
//   * ComputedStyleBase owns dense computed-value storage and base accessors.
//   * ComputedStyle adds high-level inheritance/diff/custom-property behavior.
//   * ComputedStyleBuilder is the resolver-only write interface.

#include <string>
#include <unordered_map>

#include "computed_style_base.h"

namespace style {

using CustomPropMap = std::unordered_map<std::string, std::string>;

class ComputedStyle : public ComputedStyleBase {
public:
  ComputedStyle() = default;

  void InheritFrom(const ComputedStyle& parent) {
    InheritDataFrom(parent);
    customProps_ = parent.customProps_;
    ResetNonInheritedData();
  }

  void inheritFrom(const ComputedStyle& parent) {
    InheritFrom(parent);
  }

  bool inheritedGroupsShareWith(const ComputedStyle& o) const {
    return InheritedDataShared(o);
  }

  const DataRef<StyleInheritedData>& textRef() const {
    return inheritedDataRef();
  }
  const DataRef<StyleInheritedOtherData>& inheritedOtherRef() const {
    return inheritedOtherDataRef();
  }
  const DataRef<StyleBoxData>& boxRef() const {
    return boxDataRef();
  }
  const DataRef<StyleSurroundData>& surroundRef() const {
    return surroundDataRef();
  }
  const DataRef<StyleVisualData>& visualRef() const {
    return visualDataRef();
  }

  const CustomPropMap* customProps() const {
    return customProps_;
  }

  void setCustomPropsForArena(const CustomPropMap* m) {
    customProps_ = m;
  }

private:
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
  StyleInheritedOtherData& InheritedOtherData() {
    return style_.MutableInheritedOtherData(heap_);
  }
  StyleBoxData& BoxData() {
    return style_.MutableBoxData(heap_);
  }
  StyleSurroundData& SurroundData() {
    return style_.MutableSurroundData(heap_);
  }
  StyleVisualData& VisualData() {
    return style_.MutableVisualData(heap_);
  }
  const CustomPropMap* CustomProps() const {
    return style_.customProps();
  }

  void SetCustomProps(const CustomPropMap& m) {
    style_.setCustomPropsForArena(heap_.create<CustomPropMap>(m));
  }

  CustomPropMap& MutableCustomProps() {
    const CustomPropMap* current = style_.customProps();
    CustomPropMap* copy =
        current == nullptr ? heap_.create<CustomPropMap>()
                           : heap_.create<CustomPropMap>(*current);
    style_.setCustomPropsForArena(copy);
    return *copy;
  }

  StyleInheritedData& text() {
    return InheritedData();
  }
  StyleInheritedOtherData& inheritedOther() {
    return InheritedOtherData();
  }
  StyleBoxData& box() {
    return BoxData();
  }
  StyleSurroundData& surround() {
    return SurroundData();
  }
  StyleVisualData& visual() {
    return VisualData();
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

} // namespace style
