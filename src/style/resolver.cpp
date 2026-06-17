#include "resolver.h"

#include <new>

#include "style_props.h"

#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/html/interfaces/document.h"

namespace style {

enum NodeStateFlag : uint8_t {
  kNodeSelfDirty = 1 << 0,
  kNodeSubtreeDirty = 1 << 1,
  kNodeChildDirty = 1 << 2,
};

StyleResolver::StyleResolver() {
  nodeStates_ = lexbor_dobject_create();
  if (nodeStates_ == nullptr ||
      lexbor_dobject_init(nodeStates_, 1024, sizeof(NodeState)) !=
          LXB_STATUS_OK) {
    nodeStates_ = lexbor_dobject_destroy(nodeStates_, true);
    throw std::bad_alloc();
  }
}

StyleResolver::~StyleResolver() {
  nodeStates_ = lexbor_dobject_destroy(nodeStates_, true);
}

void StyleResolver::beginDocument(const ResolveContext& rootCtx) {
  if (nodeStates_ != nullptr) {
    lexbor_dobject_clean(nodeStates_);
  }
  ++generation_;
  if (generation_ == 0) {
    generation_ = 1;
  }
  heap_.clear();
  rootCtx_ = rootCtx;
  initialStyle_ = ComputedStyle{};
}

// 单个元素的计算：继承 -> 按阶段遍历静态属性表 -> 输出 ComputedStyle。
//
// 继承属性的默认继承、非继承属性的 initial 重置都由 inheritFrom() 完成；
// 表里每个 apply_* 只在“该属性被声明”时改写对应分组，并自行处理 wide keyword。
// 阶段之间刷新 ctx（font-size 写好 em 基准、line-height 写好 lh 基准）与
// currentColor，保证后续阶段读取到已就绪的上下文。
ComputedStyle* StyleResolver::resolveElement(lxb_dom_element_t* element,
                                             const ComputedStyle& parent,
                                             const ResolveContext& parentCtx,
                                             ResolveContext& outCtx) {
  ComputedStyle* style = heap_.create<ComputedStyle>();
  style->inheritFrom(parent);
  ComputedStyleBuilder builder(*style, heap_);
  DeclaredStyle declared(element);

  outCtx = parentCtx; // 子元素上下文基线；FontSize/LineHeight 阶段写回。
  ApplyContext ac{declared, parent,  builder,
                  heap_,    outCtx,  parent.InheritedData().color};

  for (uint8_t ph = 0; ph < static_cast<uint8_t>(Phase::kCount); ++ph) {
    for (const PropDesc* d = propDescBegin(); d != propDescEnd(); ++d) {
      if (static_cast<uint8_t>(d->phase) != ph) {
        continue;
      }
      d->apply(ac);
    }
    // color 阶段结束后 currentColor 就绪，供 Normal 阶段 border/background 使用。
    if (ph == static_cast<uint8_t>(Phase::Color)) {
      ac.currentColor = style->InheritedData().color;
    }
  }
  return style;
}

void StyleResolver::releaseStyle(ComputedStyle* style) noexcept {
  if (style == nullptr) {
    return;
  }
  style->ReleaseArenaRefs(heap_);
  heap_.freePod(style);
}

void StyleResolver::resolveSubtree(lxb_dom_element_t* element,
                                   const ComputedStyle& parent,
                                   const ResolveContext& parentCtx) {
  ResolveContext outCtx;
  ComputedStyle* style = resolveElement(element, parent, parentCtx, outCtx);
  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    releaseStyle(style);
    return;
  }
  releaseStyle(state->style);
  state->style = style;
  state->context = outCtx;
  clearDirty(node);

  for (lxb_dom_node_t* child = lxb_dom_interface_node(element)->first_child;
       child != nullptr; child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      resolveSubtree(lxb_dom_interface_element(child), *style, outCtx);
    }
  }
}

const ComputedStyle*
StyleResolver::resolveElementIncremental(lxb_dom_element_t* element) {
  if (element == nullptr) {
    return nullptr;
  }

  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  const ComputedStyle* parentStyle = &initialStyle_;
  const ResolveContext* parentCtx = &rootCtx_;

  lxb_dom_node_t* parent = node->parent;
  while (parent != nullptr && parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    parent = parent->parent;
  }
  if (parent != nullptr) {
    if (const ComputedStyle* resolvedParent = styleFor(parent)) {
      parentStyle = resolvedParent;
      if (const ResolveContext* resolvedParentCtx = contextFor(parent)) {
        parentCtx = resolvedParentCtx;
      }
    }
  }

  ResolveContext outCtx;
  ComputedStyle* style =
      resolveElement(element, *parentStyle, *parentCtx, outCtx);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    releaseStyle(style);
    return nullptr;
  }
  releaseStyle(state->style);
  state->style = style;
  state->context = outCtx;
  clearDirty(node);
  return style;
}

void StyleResolver::resolveSubtreeIncremental(lxb_dom_element_t* element) {
  const ComputedStyle* style = resolveElementIncremental(element);
  if (style == nullptr) {
    return;
  }

  for (lxb_dom_node_t* child = lxb_dom_interface_node(element)->first_child;
       child != nullptr; child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      resolveSubtreeIncremental(lxb_dom_interface_element(child));
    }
  }
}

void StyleResolver::resolvePendingSubtreeIncremental(lxb_dom_node_t* root) {
  resolvePendingSubtree(root, false);
}

void StyleResolver::resolvePendingSubtree(lxb_dom_node_t* root, bool force) {
  if (root == nullptr) {
    return;
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    const bool missing = styleFor(root) == nullptr;
    const bool dirty = isDirty(root);
    force = force || dirty;
    if (missing || force) {
      resolveElementIncremental(lxb_dom_interface_element(root));
    } else if (!hasDirtyDescendant(root)) {
      return;
    }
  }

  for (lxb_dom_node_t* child = root->first_child; child != nullptr;
       child = child->next) {
    resolvePendingSubtree(child, force);
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    if (NodeState* state = ensureState(root)) {
      state->flags &= ~kNodeChildDirty;
    }
  }
}

void StyleResolver::markStyleDirty(lxb_dom_element_t* element) {
  if (element == nullptr) {
    return;
  }

  lxb_dom_node_t* node = lxb_dom_interface_node(element);
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    return;
  }
  state->flags |= kNodeSelfDirty;
  markChildDirtyAncestors(node);
}

void StyleResolver::markChildrenDirty(lxb_dom_node_t* parent) {
  if (parent == nullptr || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return;
  }

  NodeState* state = ensureState(parent);
  if (state == nullptr) {
    return;
  }
  state->flags |= kNodeChildDirty;
  markChildDirtyAncestors(parent);
}

void StyleResolver::markSubtreeStyleDirty(lxb_dom_node_t* root) {
  if (root == nullptr) {
    return;
  }

  if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    NodeState* state = ensureState(root);
    if (state == nullptr) {
      return;
    }
    state->flags |= kNodeSubtreeDirty;
    markChildDirtyAncestors(root);
    return;
  }

  for (lxb_dom_node_t* child = root->first_child; child != nullptr;
       child = child->next) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
      markSubtreeStyleDirty(child);
      return;
    }
  }
}

void StyleResolver::resolveDocument(lxb_html_document_t* doc,
                                    const ResolveContext& rootCtx) {
  beginDocument(rootCtx);
  lxb_dom_document_t* dom = lxb_dom_interface_document(doc);
  lxb_dom_element_t* root = lxb_dom_document_element(dom);
  if (root == nullptr) {
    return;
  }
  resolvePendingSubtreeIncremental(lxb_dom_interface_node(root));
}

const ComputedStyle* StyleResolver::styleFor(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state == nullptr ? nullptr : state->style;
}

StyleResolver::NodeState* StyleResolver::ensureState(lxb_dom_node_t* node) {
  if (node == nullptr || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return nullptr;
  }

  if (node->user != nullptr) {
    NodeState* state = static_cast<NodeState*>(node->user);
    if (state->owner == this && state->generation == generation_ &&
        state->node == node) {
      return state;
    }
  }

  void* storage = lexbor_dobject_calloc(nodeStates_);
  if (storage == nullptr) {
    throw std::bad_alloc();
  }

  NodeState* state = new (storage) NodeState();
  state->owner = this;
  state->node = node;
  state->generation = generation_;
  node->user = state;
  return state;
}

const StyleResolver::NodeState*
StyleResolver::stateFor(const lxb_dom_node_t* node) const {
  if (node == nullptr || node->user == nullptr) {
    return nullptr;
  }
  const NodeState* state = static_cast<const NodeState*>(node->user);
  return state->owner == this && state->generation == generation_ &&
                 state->node == node
             ? state
             : nullptr;
}

const ResolveContext*
StyleResolver::contextFor(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state == nullptr || state->style == nullptr ? nullptr
                                                     : &state->context;
}

bool StyleResolver::isDirty(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state != nullptr &&
         (state->flags & (kNodeSelfDirty | kNodeSubtreeDirty)) != 0;
}

bool StyleResolver::hasDirtyDescendant(const lxb_dom_node_t* node) const {
  const NodeState* state = stateFor(node);
  return state != nullptr && (state->flags & kNodeChildDirty) != 0;
}

void StyleResolver::clearDirty(lxb_dom_node_t* node) {
  NodeState* state = ensureState(node);
  if (state == nullptr) {
    return;
  }
  state->flags &= ~(kNodeSelfDirty | kNodeSubtreeDirty);
}

void StyleResolver::markChildDirtyAncestors(lxb_dom_node_t* node) {
  for (lxb_dom_node_t* parent = node == nullptr ? nullptr : node->parent;
       parent != nullptr && parent->type == LXB_DOM_NODE_TYPE_ELEMENT;
       parent = parent->parent) {
    NodeState* state = ensureState(parent);
    if ((state->flags & kNodeChildDirty) != 0) {
      break;
    }
    state->flags |= kNodeChildDirty;
  }
}

} // namespace style
