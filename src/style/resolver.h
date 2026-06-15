#pragma once

// StyleResolver: the self-built "second half". Walks the DOM top-down and turns
// lexbor's per-element declared values into ComputedStyle.
//
// Per element the flow is:
//   1. inheritFrom(parent)   — inherited groups shared by pointer (free);
//                              non-inherited groups reset to initial.
//   2. resolve properties in dependency-phase order (writing-mode/direction →
//      color → font-size → font props → line-height → everything else) so each
//      dependent sees ready context.
//   3. store the resulting ComputedStyle, keyed by node.
//
// A NULL declared value needs no work: step 1 already established inheritance
// (inherited props) or the initial value (non-inherited props).

#include <unordered_map>

#include "cascaded_style.h"
#include "computed_style.h"
#include "length_resolve.h"
#include "style_heap.h"

// lexbor forward types.
typedef struct lxb_html_document lxb_html_document_t;
typedef struct lxb_dom_node lxb_dom_node_t;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace style {

class StyleResolver {
public:
  StyleResolver();

  // Resolve the whole tree. `rootCtx` seeds viewport + root font metrics.
  void resolveDocument(lxb_html_document_t* doc, const ResolveContext& rootCtx);

  // Resolve a subtree rooted at `element`, given its parent's already-resolved
  // style and context. Used by both full resolve and invalidation re-resolve.
  void resolveSubtree(lxb_dom_element_t* element, const ComputedStyle& parent,
                      const ResolveContext& parentCtx);

  const ComputedStyle* styleFor(const lxb_dom_node_t* node) const;

private:
  // Resolve one element (no recursion); returns its style + outgoing context.
  ComputedStyle* resolveElement(lxb_dom_element_t* element,
                                const ComputedStyle& parent,
                                const ResolveContext& parentCtx,
                                ResolveContext& outCtx);

  StyleHeap heap_;
  CascadedStyleNormalizer cascadedStyleNormalizer_;
  std::unordered_map<const lxb_dom_node_t*, ComputedStyle*> styles_;
};

} // namespace style
