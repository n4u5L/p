#pragma once

#include <cstdint>
#include <unordered_map>

#include "lexbor/css/rule.h"
#include "lexbor/css/selectors/selector.h"

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
