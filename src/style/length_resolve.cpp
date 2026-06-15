#include "length_resolve.h"

#include "lexbor/css/value/const.h"
#include "lexbor/css/unit/const.h"

namespace style {

namespace {
// CSS 参考像素：96px == 1in。
constexpr float kPxPerIn = 96.0f;
} // 匿名命名空间

float resolveLengthPx(const lxb_css_value_length_t& len, const ResolveContext& ctx,
                      bool forFontSize) {
  const float n = static_cast<float>(len.num);
  const unsigned unit = static_cast<unsigned>(len.unit);

  switch (unit) {
  case LXB_CSS_UNIT_PX:
    return n;

  // 字体相对单位。
  case LXB_CSS_UNIT_EM:
    return n * (forFontSize ? ctx.parentFontSizePx : ctx.fontSizePx);
  case LXB_CSS_UNIT_REM:
    return n * ctx.rootFontSizePx;
  case LXB_CSS_UNIT_EX:
    return n * ctx.exPx; // TODO(M2)：接 QFontMetricsF::xHeight()。
  case LXB_CSS_UNIT_CH:
    return n * ctx.chPx; // TODO(M2)：接字符 '0' 的 advance。
  case LXB_CSS_UNIT_CAP:
  case LXB_CSS_UNIT_IC:
    return n * (forFontSize ? ctx.parentFontSizePx : ctx.fontSizePx); // TODO(M2)：改用真实字体 metrics。
  case LXB_CSS_UNIT_LH:
    return n * ctx.lineHeightPx;
  case LXB_CSS_UNIT_RLH:
    return n * ctx.rootLineHeightPx;

  // 视口相对单位。vi/vb 依赖 writing-mode；M1 暂近似为 vw/vh。
  case LXB_CSS_UNIT_VW:
  case LXB_CSS_UNIT_VI:
    return n * 0.01f * ctx.viewportW;
  case LXB_CSS_UNIT_VH:
  case LXB_CSS_UNIT_VB:
    return n * 0.01f * ctx.viewportH;
  case LXB_CSS_UNIT_VMIN:
    return n * 0.01f * (ctx.viewportW < ctx.viewportH ? ctx.viewportW : ctx.viewportH);
  case LXB_CSS_UNIT_VMAX:
    return n * 0.01f * (ctx.viewportW > ctx.viewportH ? ctx.viewportW : ctx.viewportH);

  // 绝对单位。
  case LXB_CSS_UNIT_CM:
    return n * (kPxPerIn / 2.54f);
  case LXB_CSS_UNIT_MM:
    return n * (kPxPerIn / 25.4f);
  case LXB_CSS_UNIT_Q:
    return n * (kPxPerIn / 25.4f / 4.0f);
  case LXB_CSS_UNIT_IN:
    return n * kPxPerIn;
  case LXB_CSS_UNIT_PT:
    return n * (kPxPerIn / 72.0f);
  case LXB_CSS_UNIT_PC:
    return n * (kPxPerIn / 6.0f);

  case LXB_CSS_UNIT__UNDEF:
  default:
    // 无单位值（例如裸 0）按 px 处理。
    return n;
  }
}

LengthValue resolveLengthPercentage(const lxb_css_value_length_percentage_t& lp,
                                    const ResolveContext& ctx) {
  switch (lp.type) {
  case LXB_CSS_VALUE__LENGTH:
    return LengthValue::makePx(resolveLengthPx(lp.u.length, ctx));
  case LXB_CSS_VALUE__PERCENTAGE:
    return LengthValue::makePercent(static_cast<float>(lp.u.percentage.num));
  case LXB_CSS_VALUE__NUMBER:
    // length 上的裸 number 按规范只应是 0，这里按 px 保守处理。
    return LengthValue::makePx(static_cast<float>(lp.u.length.num));
  case LXB_CSS_VALUE_AUTO:
    return LengthValue::makeAuto();
  case LXB_CSS_VALUE_NONE:
    return LengthValue::makeNone();
  default:
    // wide keyword（inherit/initial/...）应在上游处理；未知值保守退回 auto。
    return LengthValue::makeAuto();
  }
}

} // 命名空间 style
