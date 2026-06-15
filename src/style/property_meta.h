#pragma once

// 按 lexbor property id（LXB_CSS_PROPERTY_*）索引的属性元数据表。
// 这里补充 lexbor declaration 本身不携带的信息：
//   - 属性是否默认继承（lxb_css_entry_data_t 没有 inherited 标记）；
//   - computed value 写入哪个写时复制（COW）分组；
//   - 属性解析阶段，用于处理元素内部依赖顺序：
//     writing-mode -> color -> font-size -> font props -> line-height -> 其它。
//
// 表直接用 property id 下标访问，查找是 O(1)。
// 易错点：这里必须继续使用 lexbor 的 LXB_CSS_PROPERTY_*，不要再引入项目自定义
// property enum 或转换 map。

#include <cstddef>
#include <cstdint>

namespace style {

// COW 存储分组，和 computed_style.h 的数据分组保持一致。
// 分组描述写入时哪些字段一起复制/共享，和 inherited 语义是两件事。
enum class Group : uint8_t {
  InheritedText,  // color、font、text-*，继承且通常一起变化。
  InheritedOther, // direction、writing-mode、visibility。
  Box,            // display、position、sizing、flex。
  Surround,       // margin、padding、border、inset。
  Visual,         // background、opacity、overflow、decoration。
  Shorthand       // shorthand 已由 lexbor 展开，不直接存 computed value。
};

// 元素内部解析阶段。resolver 按阶段递增处理，保证 writing-mode、color、
// font-size、字体 metrics、line-height 这些上下文先于依赖它们的属性就绪。
enum class Phase : uint8_t {
  WritingContext = 0, // writing-mode、direction；逻辑边到物理边的输入。
  ColorContext = 1,   // color；currentColor 的来源。
  FontSize = 2,       // font-size；em/ex/ch 的基准。
  FontProps = 3,      // font-family/style/weight；后续接 QFont metrics。
  LineHeight = 4,     // line-height；lh 单位的基准。
  Normal = 5          // 其它属性。
};

struct PropertyMeta {
  uint16_t id = 0;        // lexbor LXB_CSS_PROPERTY_*。
  bool inherited = false; // CSS 规范定义的默认继承性。
  Group group = Group::Box;
  Phase phase = Phase::Normal;
  bool isShorthand = false; // lexbor 已展开成 longhand，resolver 跳过 shorthand。
};

// 表槽位数量：LXB_CSS_PROPERTY__LAST_ENTRY。定义放在 .cpp，避免这里泄漏 lexbor 头。
size_t propertyCount();

// O(1) 元数据查询。id 越界时返回默认元数据（非继承、Box、Normal）。
const PropertyMeta& propertyMeta(uint16_t id);

inline bool isInherited(uint16_t id) {
  return propertyMeta(id).inherited;
}

// resolver 分阶段循环使用的辅助判断；实现里直接匹配 lexbor enum 值。
bool isColorProperty(uint16_t id);  // 值里可能包含 currentColor。
bool isLengthProperty(uint16_t id); // 值大致属于 <length-percentage>。

} // 命名空间 style
