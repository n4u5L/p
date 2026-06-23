#include "layout/layout_internal.h"

#include <string.h>

#include "lexbor/css/property/const.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/text.h"

typedef enum {
  LAYOUT_TREE_BUILDER_SKIP_SELF,
  LAYOUT_TREE_BUILDER_SKIP_SUBTREE,
  LAYOUT_TREE_BUILDER_ATTACH_OBJECT
} layout_tree_builder_action_t;

typedef struct layout_tree_attach_context {
  layout_tree_t* tree;
  layout_tree_node_t* parent;
  bool reject_existing_record;
} layout_tree_attach_context_t;

typedef struct layout_tree_builder_decision {
  layout_tree_builder_action_t action;
  const lxb_style_computed_t* style;
  unsigned internal_bits;
  bool can_have_children;
} layout_tree_builder_decision_t;

typedef struct layout_tree_attach_result {
  layout_tree_node_t* node;
  layout_tree_node_t* children_parent;
  bool should_traverse_children;
} layout_tree_attach_result_t;

typedef struct layout_tree_builder_created_node {
  layout_tree_node_t* node;
  layout_block_t* block;
} layout_tree_builder_created_node_t;

static const lxb_style_computed_t*
layout_tree_builder_node_style(lxb_dom_node_t* node) {
  return (const lxb_style_computed_t*)
      lxb_dom_interface_element(node)->computed_style;
}

static bool
layout_tree_builder_text_object_is_needed(lxb_dom_node_t* node,
                                          layout_tree_node_t* parent) {
  lxb_dom_text_t* text;

  if (node == NULL || node->type != LXB_DOM_NODE_TYPE_TEXT || parent == NULL
      || parent->block == NULL || parent->object == NULL) {
    return false;
  }

  text = lxb_dom_interface_text(node);
  return text->char_data.data.length != 0;
}

static bool
layout_tree_builder_style_is_block_capable(
    const lxb_style_computed_t* style) {
  const lxb_style_computed_non_inherited_t* non_inherited;

  if (style == NULL) {
    return false;
  }

  non_inherited = style->non_inherited;
  if (non_inherited == NULL) {
    return false;
  }

  if (non_inherited->display_outside == LXB_CSS_DISPLAY_BLOCK
      || non_inherited->display_inside == LXB_CSS_DISPLAY_FLOW_ROOT
      || non_inherited->display_inside == LXB_CSS_DISPLAY_FLEX
      || non_inherited->display_inside == LXB_CSS_DISPLAY_GRID
      || non_inherited->display_inside == LXB_CSS_DISPLAY_TABLE
      || non_inherited->display_list_item) {
    return true;
  }

  switch (non_inherited->display_box) {
  case LXB_CSS_DISPLAY_BLOCK:
  case LXB_CSS_DISPLAY_INLINE_BLOCK:
  case LXB_CSS_DISPLAY_INLINE_TABLE:
  case LXB_CSS_DISPLAY_INLINE_FLEX:
  case LXB_CSS_DISPLAY_INLINE_GRID:
    return true;

  default:
    return false;
  }
}

static layout_block_t*
layout_tree_builder_create_block(layout_t* layout, layout_object_t* object) {
  layout_block_t* block;

  if (layout == NULL || layout->blocks == NULL || object == NULL) {
    return NULL;
  }

  block = lexbor_dobject_calloc(layout->blocks);
  if (block == NULL) {
    return NULL;
  }

  block->object = object;
  object->bitfields |= LAYOUT_OBJECT_INTERNAL_BLOCK;
  layout_object_child_list_init(&block->children);

  return block;
}

static bool
layout_tree_builder_object_is_anonymous_block(layout_object_t* object) {
  return object != NULL
      && (object->bitfields & LAYOUT_OBJECT_INTERNAL_ANONYMOUS_BLOCK) != 0;
}

static layout_tree_node_t*
layout_tree_builder_create_anonymous_block(layout_tree_t* tree,
                                           layout_tree_node_t* parent,
                                           const lxb_style_computed_t* style) {
  layout_object_t* object;
  layout_block_t* block;
  layout_tree_node_t* node;

  if (tree == NULL || parent == NULL || parent->block == NULL
      || style == NULL) {
    return NULL;
  }

  object = layout_object_create_anonymous(tree->layout, style);
  if (object == NULL) {
    return NULL;
  }

  block = layout_tree_builder_create_block(tree->layout, object);
  if (block == NULL) {
    return NULL;
  }
  object->bitfields |= LAYOUT_OBJECT_INTERNAL_ANONYMOUS_BLOCK;

  node = layout_tree_append_record(tree, NULL, object);
  if (node == NULL) {
    return NULL;
  }
  node->block = block;

  if (layout_object_child_list_append(&parent->block->children,
                                      parent->object,
                                      object) != LXB_STATUS_OK) {
    return NULL;
  }

  return node;
}

static layout_tree_node_t*
layout_tree_builder_add_child_to_block(layout_tree_t* tree,
                                       layout_tree_node_t* parent,
                                       layout_tree_node_t* child,
                                       const lxb_style_computed_t* child_style) {
  layout_object_t* last_child;
  layout_tree_node_t* anonymous;

  if (tree == NULL || parent == NULL || parent->block == NULL
      || child == NULL) {
    return NULL;
  }

  if (child->block != NULL) {
    if (layout_object_child_list_append(&parent->block->children,
                                        parent->object,
                                        child->object) != LXB_STATUS_OK) {
      return NULL;
    }

    return child;
  }

  last_child = parent->block->children.last_child;
  if (layout_tree_builder_object_is_anonymous_block(last_child)) {
    anonymous = layout_tree_node_for_object(tree, last_child);
  } else {
    anonymous =
        layout_tree_builder_create_anonymous_block(tree, parent, child_style);
  }

  if (anonymous == NULL || anonymous->block == NULL) {
    return NULL;
  }

  if (layout_object_child_list_append(&anonymous->block->children,
                                      anonymous->object,
                                      child->object) != LXB_STATUS_OK) {
    return NULL;
  }

  return anonymous;
}

static lxb_status_t
layout_tree_builder_decide_node(layout_tree_node_t* parent,
                                lxb_dom_node_t* dom_node,
                                layout_tree_builder_decision_t* out_decision) {
  layout_tree_builder_decision_t decision;

  if (dom_node == NULL || out_decision == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(&decision, 0, sizeof(decision));
  decision.action = LAYOUT_TREE_BUILDER_SKIP_SELF;

  if (dom_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    decision.style = layout_tree_builder_node_style(dom_node);
    if (decision.style != NULL) {
      if (layout_tree_traversal_node_is_display_none(dom_node)) {
        decision.action = LAYOUT_TREE_BUILDER_SKIP_SUBTREE;
      } else if (layout_tree_traversal_node_is_display_contents(dom_node)) {
        decision.action = LAYOUT_TREE_BUILDER_SKIP_SELF;
      } else {
        decision.action = LAYOUT_TREE_BUILDER_ATTACH_OBJECT;
        decision.can_have_children =
            layout_tree_builder_style_is_block_capable(decision.style);
      }
    }
  } else if (layout_tree_builder_text_object_is_needed(dom_node, parent)) {
    decision.style = parent->object->style;
    decision.internal_bits = LAYOUT_OBJECT_INTERNAL_TEXT;
    decision.action = LAYOUT_TREE_BUILDER_ATTACH_OBJECT;
  }

  *out_decision = decision;
  return LXB_STATUS_OK;
}

static lxb_status_t
layout_tree_builder_create_layout_node(
    layout_tree_t* tree, lxb_dom_node_t* dom_node,
    layout_tree_builder_decision_t decision, bool is_tree_root,
    layout_tree_builder_created_node_t* out_created) {
  layout_object_t* object;
  layout_block_t* block = NULL;
  layout_tree_node_t* node;

  if (tree == NULL || dom_node == NULL || out_created == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(out_created, 0, sizeof(*out_created));

  object = layout_dom_node_create_layout_object(tree->layout, dom_node,
                                                decision.style,
                                                decision.internal_bits);
  if (object == NULL) {
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  layout_tree_prepare_dom_node_for_attach(dom_node);

  if (is_tree_root || decision.can_have_children) {
    block = layout_tree_builder_create_block(tree->layout, object);
    if (block == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }
  }

  node = layout_tree_append_record(tree, dom_node, object);
  if (node == NULL) {
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  node->block = block;
  out_created->node = node;
  out_created->block = block;

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_tree_builder_place_layout_node(
    const layout_tree_attach_context_t* context,
    const layout_tree_builder_created_node_t* created,
    layout_tree_builder_decision_t decision,
    layout_tree_attach_result_t* out_result) {
  layout_tree_t* tree;
  layout_tree_node_t* parent;
  layout_tree_node_t* node;

  if (context == NULL || context->tree == NULL || created == NULL
      || created->node == NULL || out_result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  (void) decision;
  tree = context->tree;
  parent = context->parent;
  node = created->node;

  if (parent == NULL) {
    if (tree->root_object != NULL) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    tree->root_object = node->object;
    node->object->bitfields |= LAYOUT_OBJECT_INTERNAL_VIEWPORT_CONTAINING_BLOCK;
    layout_object_update_style_derived_bits(node->object);
  } else {
    layout_tree_node_t* layout_parent;

    if (parent->block == NULL) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    layout_parent =
        layout_tree_builder_add_child_to_block(tree, parent, node,
                                               layout_object_style(node->object));
    if (layout_parent == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    if (created->block == NULL) {
      out_result->children_parent = layout_parent;
    }
  }

  if (created->block != NULL) {
    out_result->children_parent = node;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_tree_builder_attach_current(
    const layout_tree_attach_context_t* context,
    lxb_dom_node_t* dom_node,
    layout_tree_builder_decision_t decision,
    layout_tree_attach_result_t* out_result) {
  layout_tree_t* tree;
  layout_tree_builder_created_node_t created;
  bool is_tree_root;
  lxb_status_t status;

  if (context == NULL || context->tree == NULL || dom_node == NULL
      || out_result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  tree = context->tree;
  memset(out_result, 0, sizeof(*out_result));
  out_result->children_parent = context->parent;
  out_result->should_traverse_children = true;

  if (decision.action == LAYOUT_TREE_BUILDER_SKIP_SELF) {
    layout_dom_node_detach_layout_tree(dom_node);
    return LXB_STATUS_OK;
  }

  if (decision.action == LAYOUT_TREE_BUILDER_SKIP_SUBTREE) {
    layout_dom_node_detach_layout_subtree(dom_node);
    out_result->should_traverse_children = false;
    return LXB_STATUS_OK;
  }

  if (decision.action != LAYOUT_TREE_BUILDER_ATTACH_OBJECT
      || decision.style == NULL) {
    layout_dom_node_detach_layout_tree(dom_node);
    return LXB_STATUS_OK;
  }

  if (context->reject_existing_record
      && layout_tree_node_for_dom_node(tree, dom_node) != NULL) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  is_tree_root = context->parent == NULL && tree->root_object == NULL;
  status = layout_tree_builder_create_layout_node(tree, dom_node, decision,
                                                  is_tree_root, &created);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  out_result->node = created.node;
  return layout_tree_builder_place_layout_node(context, &created, decision,
                                               out_result);
}

lxb_status_t
layout_tree_builder_attach_subtree(layout_tree_t* tree, lxb_dom_node_t* node,
                                   layout_tree_node_t* parent,
                                   bool reject_existing_record) {
  layout_tree_builder_decision_t decision;
  layout_tree_attach_context_t context;
  layout_tree_attach_result_t result;
  lxb_status_t status;

  if (tree == NULL || node == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_tree_builder_decide_node(parent, node, &decision);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  context.tree = tree;
  context.parent = parent;
  context.reject_existing_record = reject_existing_record;

  status = layout_tree_builder_attach_current(&context, node, decision, &result);
  if (status != LXB_STATUS_OK || !result.should_traverse_children) {
    return status;
  }

  for (lxb_dom_node_t* child = layout_tree_traversal_first_layout_child(node);
       child != NULL; child = layout_tree_traversal_next_layout_sibling(child)) {
    status = layout_tree_builder_attach_subtree(tree, child,
                                                result.children_parent,
                                                reject_existing_record);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  return LXB_STATUS_OK;
}
