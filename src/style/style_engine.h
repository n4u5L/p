#pragma once

#include <cstddef>

#include "lexbor/dom/interfaces/document.h"

#include "resolver.h"

typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_dom_document lxb_dom_document_t;
typedef struct lxb_dom_node lxb_dom_node_t;

namespace style {

// StyleResolver 外层的轻量调度器。
//
// Lexbor 负责 DOM 节点、已解析 stylesheet、以及 declaration 挂接。
// 这里不接管任何 Lexbor 对象所有权，只跟踪 stylesheet 列表变化，
// 并驱动 computed-style invalidation/recalc。
//
// 易错点：
// - 不要绕过 Lexbor 原 mutation callback，否则元素上的 declaration attach 会失效。
// - document_->node.user 被临时用来保存当前 StyleEngine，析构时必须恢复。
// - parser 关闭 <style> 和动态插入 <style> 走不同 Lexbor 路径，见 .cpp。
//
// TODO：后续可增加 selector 简单索引（tag/class/id），减少 stylesheet 插入时的
//       保守整棵文档元素子树重算。
class StyleEngine {
public:
  StyleEngine(lxb_html_document_t* document, const ResolveContext& rootCtx);
  ~StyleEngine();

  StyleEngine(const StyleEngine&) = delete;
  StyleEngine& operator=(const StyleEngine&) = delete;

  StyleResolver& resolver() {
    return resolver_;
  }
  const StyleResolver& resolver() const {
    return resolver_;
  }

  void update();
  const ComputedStyle* styleFor(const lxb_dom_node_t* node) const;

private:
  static lxb_status_t insertedCallback(lxb_dom_node_t* insertedNode);

  void installMutationCallbacks();
  void restoreMutationCallbacks();
  lxb_status_t didInsertNode(lxb_dom_node_t* insertedNode);
  void observeCurrentStylesheetsAsApplied();
  void syncStylesheets();
  lxb_dom_node_t* documentElementNode() const;
  size_t stylesheetCount() const;

  lxb_dom_document_t* document_ = nullptr;
  const lxb_dom_document_mutation_cb_t* previousMutation_ = nullptr;
  lxb_dom_document_mutation_cb_t mutation_{};
  void* previousDocumentUser_ = nullptr;
  StyleResolver resolver_;
  size_t observedStylesheetCount_ = 0;
  size_t appliedStylesheetCount_ = 0;
  bool hasObservedStylesheetCount_ = false;
};

} // 命名空间 style
