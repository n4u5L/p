#pragma once

// 静态 per-property handler 表 —— 样式引擎的“计算后半段”核心。
//
// 仿照 lexbor 的 lxb_css_property_data[]：每个 CSS 属性对应表里一行
// （PropDesc），携带元数据（id、是否继承、解析阶段）和一个 apply 回调。
// resolver 按阶段递增遍历这张表，对每个属性调用其 apply，把 lexbor 已级联的
// 声明值写进 ComputedStyle 的 COW 分组。
//
// 这取代了过去 1000+ 行过程式 resolveElement 里硬编码的 if/switch 链：
//   * 新增属性 = 在表里加一行 + 写一个小 apply 函数；
//   * 阶段顺序（writing-mode -> color -> font-size -> font -> line-height ->
//     其它）由 Phase 字段声明式表达，不再靠代码书写顺序隐式保证。
//
// 命名跟随 lexbor 习惯（snake_case 的 C 表面 + lxb_css_* 常量），实现用适度 C++。

#include <cstddef>
#include <cstdint>

#include "declared_style.h"
#include "length_resolve.h"
#include "value_types.h"

namespace style {

class ComputedStyle;
class ComputedStyleBuilder;
class StyleArena;

// 元素内部解析阶段：低阶段先算，高阶段读已就绪的上下文。
// 取代旧 property_meta.h 里的 Phase，语义一致。
enum class Phase : uint8_t {
  WritingMode = 0, // writing-mode、direction：逻辑边到物理边的输入。
  Color,           // color：currentColor 的来源。
  FontSize,        // font-size：em/ex/ch 的基准。
  Font,            // font-family/style/weight/stretch。
  LineHeight,      // line-height：lh 单位基准。
  Normal,          // 其它所有属性。
  kCount
};

// 一次属性应用的上下文，传给每个 apply 回调。
// ctx 与 currentColor 是可变的：FontSize/LineHeight 阶段写回 ctx，
// Color 阶段后 resolver 刷新 currentColor，供后续阶段读取。
struct ApplyContext {
  const DeclaredStyle& declared; // 元素级联后的声明值（lexbor AVL）。
  const ComputedStyle& parent;   // 已解析的父样式，inherit 时取值。
  ComputedStyleBuilder& builder; // 写入 COW 分组的唯一入口。
  StyleArena& heap;              // 变长数据（font-family interned string）。
  ResolveContext& ctx;           // 当前元素解析长度用的上下文（可写）。
  Color currentColor;            // 已就绪的 color，供 currentColor 关键字。
};

// per-property handler。函数指针，无虚函数/类型擦除，表可 static。
struct PropDesc {
  uint16_t id;                  // lexbor LXB_CSS_PROPERTY_*。
  bool inherited;               // CSS 规范默认继承性。
  Phase phase;                  // 解析阶段。
  void (*apply)(ApplyContext&); // 读自身声明值 + 处理 wide keyword + 写 builder。
};

// 静态表区间。resolver 用 [begin, end) 遍历。
const PropDesc* propDescBegin();
const PropDesc* propDescEnd();

} // namespace style
