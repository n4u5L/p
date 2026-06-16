#pragma once

// StyleResolver：自建样式系统的“计算后半段”。
// 它按 DOM 自顶向下，把 lexbor 挂在元素上的声明值转换成 ComputedStyle。
//
// 单个元素的流程：
//   1. inheritFrom(parent)：继承组先共享父样式指针，非继承组重置为 initial。
//   2. 按依赖阶段解析属性：
//      writing-mode/direction -> color -> font-size -> font props ->
//      line-height -> 其它属性，保证依赖方读取到已准备好的上下文。
//   3. 把 ComputedStyle 存入节点状态。
//
// 声明值为 NULL 时不用处理：步骤 1 已经完成继承属性的继承，
// 以及非继承属性的 initial 重置。
//
// 易错点：
// - 节点状态挂在元素节点的 lxb_dom_node_t::user 上；不要给 document/text
//   节点写 resolver 状态，StyleEngine 会使用 document->node.user。
// - NodeState 来自 lexbor_dobject_t 对象池。beginDocument() 会清池，
//   因此读取 node->user 时必须校验 owner/generation/node 三者。
// - Resolver 不负责 stylesheet lifecycle；stylesheet apply 与 dirty 驱动
//   放在 StyleEngine。

#include <cstdint>

#include "lexbor/core/dobject.h"

#include "cascaded_style.h"
#include "computed_style.h"
#include "length_resolve.h"
#include "style_heap.h"

// lexbor 前置声明。
typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace style {

class StyleResolver {
public:
  StyleResolver();
  ~StyleResolver();

  StyleResolver(const StyleResolver&) = delete;
  StyleResolver& operator=(const StyleResolver&) = delete;

  // 开始一个新的文档样式轮次。rootCtx 提供 viewport 和根字体等初始上下文。
  // 增量解析元素前必须先调用。
  void beginDocument(const ResolveContext& rootCtx);

  // 解析整棵树。兼容旧调用的包装，内部仍走增量入口。
  void resolveDocument(lxb_html_document_t* doc, const ResolveContext& rootCtx);

  // 解析一个新插入元素。优先使用已解析父元素的 style/context；
  // 根元素退回 beginDocument() 提供的 root context。
  const ComputedStyle* resolveElementIncremental(lxb_dom_element_t* element);

  // 按 DOM 顺序解析一棵新插入子树。
  // 目前用于整树解析，也可被后续 tree-builder hook 直接调用。
  void resolveSubtreeIncremental(lxb_dom_element_t* element);

  // 遍历文档/子树，只解析新增元素或被 invalidation 标脏的元素。
  // 干净分支通过 ChildDirty 标记跳过，避免每轮全树扫到底。
  void resolvePendingSubtreeIncremental(lxb_dom_node_t* root);

  // 标记 computed style 缓存过期。
  // markStyleDirty() 只标当前元素；markChildrenDirty() 表示父节点有新增/脏孩子；
  // markSubtreeStyleDirty() 是继承变化或 stylesheet 变化时的保守路径。
  void markStyleDirty(lxb_dom_element_t* element);
  void markChildrenDirty(lxb_dom_node_t* parent);
  void markSubtreeStyleDirty(lxb_dom_node_t* root);

  // 低层重算入口：调用方显式提供父样式和父上下文。
  void resolveSubtree(lxb_dom_element_t* element, const ComputedStyle& parent,
                      const ResolveContext& parentCtx);

  const ComputedStyle* styleFor(const lxb_dom_node_t* node) const;

private:
  // 只解析单个元素，不递归；返回 ComputedStyle，并输出传给子元素的 context。
  ComputedStyle* resolveElement(lxb_dom_element_t* element,
                                const ComputedStyle& parent,
                                const ResolveContext& parentCtx,
                                ResolveContext& outCtx);
  void resolvePendingSubtree(lxb_dom_node_t* root, bool force);

  struct NodeState {
    const StyleResolver* owner = nullptr;
    const lxb_dom_node_t* node = nullptr;
    ComputedStyle* style = nullptr;
    ResolveContext context;
    uint32_t generation = 0;
    uint8_t flags = 0;
  };

  NodeState* ensureState(lxb_dom_node_t* node);
  const NodeState* stateFor(const lxb_dom_node_t* node) const;
  const ResolveContext* contextFor(const lxb_dom_node_t* node) const;
  bool isDirty(const lxb_dom_node_t* node) const;
  bool hasDirtyDescendant(const lxb_dom_node_t* node) const;
  void clearDirty(lxb_dom_node_t* node);
  void markChildDirtyAncestors(lxb_dom_node_t* node);
  void releaseStyle(ComputedStyle* style) noexcept;

  StyleHeap heap_;
  CascadedStyleNormalizer cascadedStyleNormalizer_;
  lexbor_dobject_t* nodeStates_ = nullptr;
  uint32_t generation_ = 1;
  ComputedStyle initialStyle_;
  ResolveContext rootCtx_;
};

} // namespace style
