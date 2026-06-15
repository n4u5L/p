#include "style_engine.h"

#include "lexbor/core/array.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/style/dom/interfaces/document.h"

namespace style {

StyleEngine::StyleEngine(lxb_html_document_t* document,
                         const ResolveContext& rootCtx)
    : document_(lxb_dom_interface_document(document)) {
  resolver_.beginDocument(rootCtx);
  installMutationCallbacks();
}

StyleEngine::~StyleEngine() {
  restoreMutationCallbacks();
}

void StyleEngine::update() {
  syncStylesheets();

  lxb_dom_node_t* root = documentElementNode();
  if (root != nullptr) {
    resolver_.resolvePendingSubtreeIncremental(root);
  }
}

const ComputedStyle* StyleEngine::styleFor(const lxb_dom_node_t* node) const {
  return resolver_.styleFor(node);
}

// 安装在 Lexbor document mutation 上的薄包装。
//
// 易错点：Lexbor 的 style 模块已经安装了自己的 callback，用来给新元素挂接
// stylesheet declaration。这里必须先调用旧 callback，再做我们的 computed
// style dirty 标记；否则 resolver 会看到空 declaration。
void StyleEngine::installMutationCallbacks() {
  if (document_ == nullptr) {
    return;
  }

  previousMutation_ = document_->mutation;
  previousDocumentUser_ = document_->node.user;
  mutation_ = previousMutation_ != nullptr ? *previousMutation_
                                           : lxb_dom_document_mutation_cb_t{};
  mutation_.inserted = insertedCallback;
  document_->node.user = this;
  document_->mutation = &mutation_;
}

void StyleEngine::restoreMutationCallbacks() {
  if (document_ == nullptr || document_->mutation != &mutation_) {
    return;
  }

  document_->mutation = previousMutation_;
  document_->node.user = previousDocumentUser_;
}

lxb_status_t StyleEngine::insertedCallback(lxb_dom_node_t* insertedNode) {
  if (insertedNode == nullptr || insertedNode->owner_document == nullptr) {
    return LXB_STATUS_OK;
  }

  auto* engine = static_cast<StyleEngine*>(
      lxb_dom_interface_node(insertedNode->owner_document)->user);
  return engine == nullptr ? LXB_STATUS_OK : engine->didInsertNode(insertedNode);
}

lxb_status_t StyleEngine::didInsertNode(lxb_dom_node_t* insertedNode) {
  lxb_status_t status = LXB_STATUS_OK;
  if (previousMutation_ != nullptr && previousMutation_->inserted != nullptr) {
    status = previousMutation_->inserted(insertedNode);
  }

  if (insertedNode->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    // 新元素已经插入到父节点下，只需要标记父链有脏孩子。
    // 真正解析时从 documentElement 进入，干净分支会被 ChildDirty 跳过。
    resolver_.markChildrenDirty(insertedNode->parent);
  }

  const size_t count = stylesheetCount();
  if (hasObservedStylesheetCount_ && count != observedStylesheetCount_) {
    // 动态插入 <style> 时 Lexbor 走 stylesheet_attach()，已经 apply 到现有 DOM。
    // 这里只同步计数并标记 computed style 过期，不能再重复 apply。
    observeCurrentStylesheetsAsApplied();
    if (lxb_dom_node_t* root = documentElementNode()) {
      resolver_.markSubtreeStyleDirty(root);
    }
  }

  return status;
}

void StyleEngine::observeCurrentStylesheetsAsApplied() {
  const size_t count = stylesheetCount();
  observedStylesheetCount_ = count;
  appliedStylesheetCount_ = count;
  hasObservedStylesheetCount_ = true;
}

void StyleEngine::syncStylesheets() {
  const size_t count = stylesheetCount();
  if (!hasObservedStylesheetCount_) {
    observeCurrentStylesheetsAsApplied();
    return;
  }

  if (count == observedStylesheetCount_) {
    return;
  }

  // 解析器关闭 <style> 时走 stylesheet_add()：新 stylesheet 只进入列表，
  // 尚未 apply 到已有元素。动态插入 <style> 走 stylesheet_attach()：
  // didInsertNode() 已经把这条路径记录为 applied，避免这里重复 apply。
  //
  // 易错点：重复 apply 同一个 stylesheet 会把旧 declaration 挂到 weak 链，
  // 既浪费内存，也可能让级联调试变复杂。
  for (size_t i = appliedStylesheetCount_; i < count; ++i) {
    auto* stylesheet = static_cast<lxb_css_stylesheet_t*>(
        lexbor_array_get(document_->css->stylesheets, i));
    if (stylesheet != nullptr) {
      (void) lxb_dom_document_stylesheet_apply(document_, stylesheet);
    }
  }

  if (lxb_dom_node_t* root = documentElementNode()) {
    resolver_.markSubtreeStyleDirty(root);
  }

  observedStylesheetCount_ = count;
  appliedStylesheetCount_ = count;
}

lxb_dom_node_t* StyleEngine::documentElementNode() const {
  if (document_ == nullptr) {
    return nullptr;
  }

  lxb_dom_element_t* element = lxb_dom_document_element(document_);
  return element == nullptr ? nullptr : lxb_dom_interface_node(element);
}

size_t StyleEngine::stylesheetCount() const {
  if (document_ == nullptr || document_->css == nullptr ||
      document_->css->stylesheets == nullptr) {
    return 0;
  }

  return lexbor_array_length(document_->css->stylesheets);
}

} // 命名空间 style
