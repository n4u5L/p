#pragma once

// DeclaredStyle 是 resolver 读取“级联后声明值”的薄封装。
//
// lexbor 已经完成 selector matching 和级联：每个元素的 element->style 是一棵
// AVL，按 property id 存放“胜出”的 declaration（含 inline style）。这里直接查它，
// 取代旧的 std::unordered_map CascadedStyle —— 不再为每个元素分配哈希表。
//
// 易错点：
// - declaration 指针仍归 lexbor stylesheet 所有，这里只读不放，也不要复制 CSS 值。
// - lexbor 把 shorthand（margin/padding/border）和 longhand 存成不同 property id 的
//   独立节点，不会互相展开。shorthand -> longhand 的归一在 resolver 内做
//   （declaredLength / declaredBorder），这里只提供按 id 的 O(1) 查询与 specificity。

#include <cstdint>

#include "lexbor/css/rule.h"
#include "lexbor/css/selectors/selector.h"
#include "lexbor/style/base.h"
#include "lexbor/style/dom/interfaces/element.h"

typedef struct lxb_dom_element lxb_dom_element_t;

namespace style {

class DeclaredStyle {
public:
  explicit DeclaredStyle(const lxb_dom_element_t* element) noexcept
      : element_(element) {
  }

  // 按 canonical property id 取胜出的 declaration；没有则返回 nullptr。
  const lxb_css_rule_declaration_t* declaration(uint16_t id) const noexcept {
    if (element_ == nullptr) {
      return nullptr;
    }
    return lxb_dom_element_style_by_id(element_, id);
  }

  const lxb_css_rule_declaration_t* rawDeclaration(uint16_t id) const noexcept {
    const lxb_css_rule_declaration_t* d = declaration(id);
    return d != nullptr && d->type == LXB_CSS_PROPERTY__UNDEF &&
                   d->u.undef != nullptr && d->u.undef->type == id
               ? d
               : nullptr;
  }

  // 该 property id 胜出 declaration 的 specificity；用于 shorthand/longhand 仲裁。
  // 没有声明时返回 0（最低）。
  lxb_css_selector_specificity_t specificity(uint16_t id) const noexcept {
    if (element_ == nullptr) {
      return 0;
    }
    const lxb_style_node_t* node = lxb_dom_element_style_node_by_id(element_, id);
    return node == nullptr ? 0 : node->sp;
  }

  // 取胜出 declaration 的 typed value。lexbor 的 declaration union 成员都别名到
  // u.user，所以直接 cast 即可。
  template <class T>
  const T* value(uint16_t id) const noexcept {
    const lxb_css_rule_declaration_t* d = declaration(id);
    return d != nullptr && d->type == id ? static_cast<const T*>(d->u.user)
                                         : nullptr;
  }

private:
  const lxb_dom_element_t* element_;
};

} // namespace style
