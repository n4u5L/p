#pragma once

#include <cstdint>
#include <unordered_map>

#include "lexbor/css/rule.h"
#include "lexbor/css/selectors/selector.h"

// CascadedStyle 是 resolver 前面的轻量归一化层。
// lexbor 已经完成 selector matching 和 declaration 挂接，这里只把元素上胜出的
// declaration 收集成 canonical longhand -> declaration 的表。
//
// 易错点：
// - declaration 指针仍归 lexbor stylesheet 所有，这里不能释放，也不要复制 CSS 值。
// - shorthand 只展开到本项目当前需要的 canonical longhand；缺项要显式 TODO 补。
// - 继续使用 lexbor 的 property id，避免再做一层 enum map。
// TODO：为低内存目标，后续可把 unordered_map 换成小数组/排序向量或 arena 分配容器。

typedef struct lxb_dom_element lxb_dom_element_t;

namespace style {

struct CascadedDeclaration {
  const lxb_css_rule_declaration_t* declaration = nullptr;
  lxb_css_selector_specificity_t specificity = 0;
  uint16_t sourceProperty = 0;
};

class CascadedStyle {
public:
  bool has(uint16_t id) const {
    return declarations_.find(id) != declarations_.end();
  }

  const CascadedDeclaration* declaration(uint16_t id) const {
    auto it = declarations_.find(id);
    return it == declarations_.end() ? nullptr : &it->second;
  }

  template <class T>
  const T* value(uint16_t id) const {
    const CascadedDeclaration* d = declaration(id);
    return d != nullptr && d->declaration != nullptr
               ? static_cast<const T*>(d->declaration->u.user)
               : nullptr;
  }

  void set(uint16_t id, const CascadedDeclaration& declaration) {
    auto it = declarations_.find(id);
    if (it == declarations_.end() ||
        declaration.specificity >= it->second.specificity) {
      declarations_[id] = declaration;
    }
  }

private:
  std::unordered_map<uint16_t, CascadedDeclaration> declarations_;
};

class CascadedStyleNormalizer {
public:
  CascadedStyle collect(lxb_dom_element_t* element) const;
};

} // namespace style
