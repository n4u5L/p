#include "length_resolve.h"

#include "lexbor/css/value/const.h"
#include "lexbor/css/unit/const.h"

namespace style {

namespace {
// CSS reference pixel: 96px == 1in.
constexpr float kPxPerIn = 96.0f;
} // namespace

float resolveLengthPx(const lxb_css_value_length_t& len, const ResolveContext& ctx,
                      bool forFontSize) {
  const float n = static_cast<float>(len.num);

  switch (len.unit) {
  case LXB_CSS_UNIT_PX:
    return n;

  // Font-relative.
  case LXB_CSS_UNIT_EM:
    return n * (forFontSize ? ctx.parentFontSizePx : ctx.fontSizePx);
  case LXB_CSS_UNIT_REM:
    return n * ctx.rootFontSizePx;
  case LXB_CSS_UNIT_EX:
    return n * ctx.exPx; // M2: QFontMetricsF::xHeight()
  case LXB_CSS_UNIT_CH:
    return n * ctx.chPx; // M2: advance of '0'
  case LXB_CSS_UNIT_CAP:
  case LXB_CSS_UNIT_IC:
    return n * (forFontSize ? ctx.parentFontSizePx : ctx.fontSizePx); // M2: real metrics
  case LXB_CSS_UNIT_LH:
    return n * ctx.lineHeightPx;
  case LXB_CSS_UNIT_RLH:
    return n * ctx.rootLineHeightPx;

  // Viewport-relative. vi/vb depend on writing-mode; M1 approximates to vw/vh.
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

  // Absolute.
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
    // Unitless (e.g. a bare `0`) — treat the number as pixels.
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
    // bare number in a length context (only valid as 0) → pixels.
    return LengthValue::makePx(static_cast<float>(lp.u.length.num));
  case LXB_CSS_VALUE_AUTO:
    return LengthValue::makeAuto();
  case LXB_CSS_VALUE_NONE:
    return LengthValue::makeNone();
  default:
    // Wide keywords (inherit/initial/...) are handled upstream; anything
    // unexpected falls back to auto.
    return LengthValue::makeAuto();
  }
}

} // namespace style
