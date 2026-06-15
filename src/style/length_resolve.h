#pragma once

// 长度单位解析：把 lexbor 解析阶段的 <length>（数值 + 单位）转换成
// computed 阶段使用的 LengthValue。百分比不会在这里算成 px，而是保留为
// LengthValue::Percent，交给 layout 结合包含块计算 used value。
//
// 阶段范围：
//   M1：px、em、rem、绝对单位（cm/mm/in/pt/pc/Q）正常解析；ex/ch/视口单位
//       暂用明确的近似值。
//   TODO(M2)：用 QFontMetricsF 和真实 viewport 支持 ex/ch/cap/ic/lh/rlh 以及
//             vw/vh/vi/vb/vmin/vmax。
//
// 易错点：computed value 不是 used value。不要为了“方便 layout”在这里提前消掉
// 百分比、auto 或后续 calc()，否则布局阶段会丢失必要上下文。

#include "value_types.h"

// lexbor C 值结构体。
#include "lexbor/css/value.h"

namespace style {

// 单位解析需要的元素上下文。
struct ResolveContext {
  float fontSizePx = 16.0f;       // 当前元素 computed font-size，供 em 使用。
  float parentFontSizePx = 16.0f; // 父元素 font-size，供 font-size 上的 em 使用。
  float rootFontSizePx = 16.0f;   // 根元素 font-size，供 rem 使用。
  float lineHeightPx = 19.2f;     // 当前 computed line-height，供 lh 使用。
  float rootLineHeightPx = 19.2f; // 根元素 line-height，供 rlh 使用。
  float viewportW = 0.0f;         // viewport 宽度 px，供 vw/vi/vmin/vmax 使用。
  float viewportH = 0.0f;         // viewport 高度 px，供 vh/vb/vmin/vmax 使用。

  // TODO(M2)：这些应来自当前字体的 QFontMetricsF。
  float exPx = 8.0f; // x-height；M2 前近似为 0.5em。
  float chPx = 8.0f; // 字符 '0' 的 advance；M2 前近似为 0.5em。
};

// 把纯 <length> 解析成绝对 px。forFontSize 控制 em 的基准：
// font-size 属性用父字号，其它属性用当前元素字号。
float resolveLengthPx(const lxb_css_value_length_t& len, const ResolveContext& ctx,
                      bool forFontSize = false);

// 把 <length-percentage> typed value 解析成 computed LengthValue。
// auto/none/inherit 等关键字由 resolver 先处理；这里正常只接收具体长度或百分比。
LengthValue resolveLengthPercentage(const lxb_css_value_length_percentage_t& lp,
                                    const ResolveContext& ctx);

} // 命名空间 style
