#include "cascaded_style.h"

#include "lexbor/core/avl.h"
#include "lexbor/css/property/const.h"
#include "lexbor/style/base.h"
#include "lexbor/style/dom/interfaces/element.h"

namespace style {

namespace {

struct WalkContext {
  CascadedStyle* style = nullptr;
};

// 把源 declaration 按 canonical property id 放入表中。
// sourceProperty 保留原始属性，方便 resolver 知道它来自 shorthand 还是 longhand。
void addDeclaration(CascadedStyle& style,
                    const lxb_css_rule_declaration_t* declaration,
                    lxb_css_selector_specificity_t specificity,
                    uint16_t canonicalId) {
  style.set(canonicalId,
            CascadedDeclaration{declaration, specificity, static_cast<uint16_t>(declaration->type)});
}

// 四边 shorthand 的 M1 展开路径；实际值展开仍在 resolver 中按 CSS 四值规则处理。
void addExpanded(CascadedStyle& style, const lxb_css_rule_declaration_t* declaration,
                 lxb_css_selector_specificity_t specificity, uint16_t top,
                 uint16_t right, uint16_t bottom, uint16_t left) {
  addDeclaration(style, declaration, specificity, top);
  addDeclaration(style, declaration, specificity, right);
  addDeclaration(style, declaration, specificity, bottom);
  addDeclaration(style, declaration, specificity, left);
}

// border shorthand 先映射到物理边，具体 width/style/color 后续由 resolver 拆。
void addBorderEdge(CascadedStyle& style,
                   const lxb_css_rule_declaration_t* declaration,
                   lxb_css_selector_specificity_t specificity,
                   uint16_t edgeId) {
  addDeclaration(style, declaration, specificity, edgeId);
}

// 将 lexbor property id 归一到 resolver 要读取的 canonical id。
// TODO：补齐更多 shorthand 时优先在这里扩展，不要在 resolver 里到处特判。
void addCanonical(CascadedStyle& style,
                  const lxb_css_rule_declaration_t* declaration,
                  lxb_css_selector_specificity_t specificity) {
  switch (declaration->type) {
  case LXB_CSS_PROPERTY_MARGIN:
    addExpanded(style, declaration, specificity, LXB_CSS_PROPERTY_MARGIN_TOP, LXB_CSS_PROPERTY_MARGIN_RIGHT, LXB_CSS_PROPERTY_MARGIN_BOTTOM, LXB_CSS_PROPERTY_MARGIN_LEFT);
    break;
  case LXB_CSS_PROPERTY_PADDING:
    addExpanded(style, declaration, specificity, LXB_CSS_PROPERTY_PADDING_TOP, LXB_CSS_PROPERTY_PADDING_RIGHT, LXB_CSS_PROPERTY_PADDING_BOTTOM, LXB_CSS_PROPERTY_PADDING_LEFT);
    break;
  case LXB_CSS_PROPERTY_BORDER:
    addExpanded(style, declaration, specificity, LXB_CSS_PROPERTY_BORDER_TOP, LXB_CSS_PROPERTY_BORDER_RIGHT, LXB_CSS_PROPERTY_BORDER_BOTTOM, LXB_CSS_PROPERTY_BORDER_LEFT);
    break;
  case LXB_CSS_PROPERTY_BORDER_TOP:
  case LXB_CSS_PROPERTY_BORDER_RIGHT:
  case LXB_CSS_PROPERTY_BORDER_BOTTOM:
  case LXB_CSS_PROPERTY_BORDER_LEFT:
    addBorderEdge(style, declaration, specificity, static_cast<uint16_t>(declaration->type));
    break;
  default:
    addDeclaration(style, declaration, specificity, static_cast<uint16_t>(declaration->type));
    break;
  }
}

lxb_status_t walkStyle(lxb_dom_element_t*,
                       const lxb_css_rule_declaration_t* declaration,
                       void* ctx,
                       lxb_css_selector_specificity_t specificity,
                       bool) {
  auto* walk = static_cast<WalkContext*>(ctx);

  addCanonical(*walk->style, declaration, specificity);

  return LXB_STATUS_OK;
}

} // namespace

CascadedStyle CascadedStyleNormalizer::collect(lxb_dom_element_t* element) const {
  CascadedStyle style;
  if (element == nullptr || element->style == nullptr) {
    return style;
  }

  WalkContext ctx{&style};
  lxb_dom_element_style_walk(element, walkStyle, &ctx, true);
  return style;
}

} // namespace style
