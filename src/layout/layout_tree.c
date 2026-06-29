#include "layout/layout_internal.h"
#include "layout/layout_algorithm.h"

#include <string.h>

#include "lexbor/core/lexbor.h"

layout_tree_node_t*
layout_tree_node_for_object(layout_tree_t* tree, layout_object_t* object) {
  size_t length;

  if (tree == NULL || tree->nodes == NULL || object == NULL) {
    return NULL;
  }

  length = lexbor_dobject_allocated(tree->nodes);
  for (size_t i = 0; i < length; i++) {
    layout_tree_node_t* node =
        lexbor_dobject_by_absolute_position(tree->nodes, i);

    if (node != NULL && node->object == object) {
      return node;
    }
  }

  return NULL;
}

layout_tree_node_t*
layout_tree_node_for_dom_node(layout_tree_t* tree, lxb_dom_node_t* dom_node) {
  size_t length;

  if (tree == NULL || tree->nodes == NULL || dom_node == NULL) {
    return NULL;
  }

  length = lexbor_dobject_allocated(tree->nodes);
  for (size_t i = 0; i < length; i++) {
    layout_tree_node_t* node =
        lexbor_dobject_by_absolute_position(tree->nodes, i);

    if (node != NULL && node->node == dom_node) {
      return node;
    }
  }

  return NULL;
}

layout_tree_node_t*
layout_tree_append_record(layout_tree_t* tree, lxb_dom_node_t* dom_node,
                          layout_object_t* object) {
  layout_tree_node_t* node;

  if (tree == NULL || tree->nodes == NULL || object == NULL) {
    return NULL;
  }

  node = lexbor_dobject_calloc(tree->nodes);
  if (node == NULL) {
    return NULL;
  }

  node->node = dom_node;
  node->object = object;

  return node;
}

layout_tree_t*
layout_tree_create(layout_t* layout) {
  layout_tree_t* tree;

  if (layout == NULL) {
    return NULL;
  }

  tree = lexbor_calloc(1, sizeof(layout_tree_t));
  if (tree == NULL) {
    return NULL;
  }

  if (layout_tree_init(tree, layout) != LXB_STATUS_OK) {
    return layout_tree_destroy(tree, true);
  }

  return tree;
}

lxb_status_t
layout_tree_init(layout_tree_t* tree, layout_t* layout) {
  lxb_status_t status;

  if (tree == NULL || layout == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(tree, 0, sizeof(layout_tree_t));
  tree->layout = layout;
  tree->nodes = lexbor_dobject_create();
  if (tree->nodes == NULL) {
    layout_tree_destroy(tree, false);
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  status = lexbor_dobject_init(tree->nodes, LAYOUT_TREE_NODE_CHUNK, sizeof(layout_tree_node_t));
  if (status != LXB_STATUS_OK) {
    layout_tree_destroy(tree, false);
    return status;
  }

  return LXB_STATUS_OK;
}

layout_tree_t*
layout_tree_destroy(layout_tree_t* tree, bool destroy_self) {
  if (tree == NULL) {
    return NULL;
  }

  tree->nodes = lexbor_dobject_destroy(tree->nodes, true);

  if (destroy_self) {
    return lexbor_free(tree);
  }

  return tree;
}

void layout_tree_clean(layout_tree_t* tree) {
  if (tree == NULL) {
    return;
  }

  if (tree->nodes != NULL) {
    lexbor_dobject_clean(tree->nodes);
  }

  tree->root_object = NULL;
}

layout_t*
layout_tree_layout(layout_tree_t* tree) {
  return tree == NULL ? NULL : tree->layout;
}

static void
layout_tree_detach_skipped_subtree(lxb_dom_node_t* root_node) {
  bool display_none;
  bool display_contents;

  if (root_node == NULL) {
    return;
  }

  display_none = layout_internal_node_is_display_none(root_node);
  display_contents = !display_none
                     && layout_internal_node_is_display_contents(root_node);

  if (display_none) {
    layout_dom_node_detach_layout_subtree(root_node);
    return;
  }

  if (display_contents) {
    layout_dom_node_detach_layout_tree(root_node);
  }

  for (lxb_dom_node_t* child = root_node->first_child; child; child = child->next) {
    layout_tree_detach_skipped_subtree(child);
  }
}

lxb_status_t
layout_tree_build(layout_tree_t* tree, lxb_dom_node_t* root_node) {
  lxb_status_t status;

  if (tree == NULL || root_node == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (tree->layout == NULL || tree->layout->lifecycle_postponed) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  status = layout_lifecycle_advance_to(tree->layout,
                                       LAYOUT_LIFECYCLE_IN_STYLE_RECALC);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  layout_tree_clean(tree);
  layout_tree_detach_skipped_subtree(root_node);

  status = layout_tree_builder_attach_subtree(tree, root_node, NULL, false);
  if (status != LXB_STATUS_OK) {
    layout_tree_clean(tree);
    layout_lifecycle_abort_update(tree->layout);
    return status;
  }

  return layout_lifecycle_advance_to(tree->layout,
                                     LAYOUT_LIFECYCLE_STYLE_CLEAN);
}

layout_object_t*
layout_tree_root_object(layout_tree_t* tree) {
  return tree == NULL ? NULL : tree->root_object;
}

lxb_status_t
layout_dom_node_attach_layout_tree(layout_tree_t* tree, lxb_dom_node_t* node,
                                   layout_object_t* parent) {
  layout_tree_node_t* parent_node = NULL;
  lxb_status_t status;

  if (tree == NULL || node == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (tree->layout == NULL || tree->layout->lifecycle_postponed) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  status = layout_lifecycle_advance_to(tree->layout,
                                       LAYOUT_LIFECYCLE_IN_STYLE_RECALC);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (parent != NULL) {
    parent_node = layout_tree_node_for_object(tree, parent);
    if (parent_node == NULL) {
      layout_lifecycle_abort_update(tree->layout);
      return LXB_STATUS_ERROR_NOT_EXISTS;
    }
    if (parent_node->block == NULL) {
      layout_lifecycle_abort_update(tree->layout);
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }
  }

  status = layout_tree_builder_attach_subtree(tree, node, parent_node, true);
  if (status != LXB_STATUS_OK) {
    layout_lifecycle_abort_update(tree->layout);
    return status;
  }

  return layout_lifecycle_advance_to(tree->layout,
                                     LAYOUT_LIFECYCLE_STYLE_CLEAN);
}

void layout_dom_node_detach_layout_tree(lxb_dom_node_t* node) {
  layout_object_t* object;
  layout_t* layout = NULL;
  bool scoped_detach = false;

  if (node == NULL) {
    return;
  }

  object = (layout_object_t*)node->layout_object;
  if (object != NULL) {
    layout = object->layout;
    if (layout != NULL && !layout_lifecycle_allows_layout_tree_mutations(layout)) {
      if (layout_lifecycle_begin_detach(layout) != LXB_STATUS_OK) {
        return;
      }
      scoped_detach = true;
    }
  }

  node->layout_object = NULL;

  if (object == NULL || layout_object_is_anonymous(object)) {
    if (scoped_detach) {
      layout_lifecycle_end_detach(layout);
      layout_lifecycle_mark_update_pending(layout);
    }
    return;
  }

  layout_object_unlink_from_siblings(object);
  layout_object_detach_box(object);
  object->parent = NULL;
  object->native_embed_token = 0;
  object->dirty_bits = LAYOUT_DIRTY_NONE;
  object->bitfields &= LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS;

  if (object->style != NULL) {
    lxb_style_computed_unref((lxb_style_computed_t*)object->style);
    object->style = NULL;
  }

  object->node = NULL;

  if (scoped_detach) {
    layout_lifecycle_end_detach(layout);
    layout_lifecycle_mark_update_pending(layout);
  }
}

void layout_dom_node_detach_layout_subtree(lxb_dom_node_t* root_node) {
  if (root_node == NULL) {
    return;
  }

  for (lxb_dom_node_t* child = root_node->first_child; child; child = child->next) {
    layout_dom_node_detach_layout_subtree(child);
  }

  layout_dom_node_detach_layout_tree(root_node);
}

layout_object_t*
layout_tree_object_for_node(layout_tree_t* tree, lxb_dom_node_t* node) {
  layout_tree_node_t* tree_node = layout_tree_node_for_dom_node(tree, node);

  return tree_node == NULL ? NULL : tree_node->object;
}

layout_object_child_list_t*
layout_tree_children(layout_tree_t* tree, layout_object_t* object) {
  layout_tree_node_t* tree_node = layout_tree_node_for_object(tree, object);

  if (tree_node == NULL || tree_node->block == NULL) {
    return NULL;
  }

  return &tree_node->block->children;
}

static bool
layout_tree_object_is_recorded(layout_tree_t* tree, layout_object_t* object) {
  return layout_tree_node_for_object(tree, object) != NULL;
}

static bool
layout_tree_object_is_within(layout_object_t* object,
                             layout_object_t* ancestor) {
  if (ancestor == NULL) {
    return true;
  }

  for (layout_object_t* current = object; current != NULL;
       current = current->parent) {
    if (current == ancestor) {
      return true;
    }
  }

  return false;
}

static bool
layout_tree_object_traversal_args_valid(layout_tree_t* tree,
                                        layout_object_t* object,
                                        layout_object_t* stay_within) {
  if (!layout_tree_object_is_recorded(tree, object)) {
    return false;
  }

  if (stay_within != NULL && !layout_tree_object_is_recorded(tree, stay_within)) {
    return false;
  }

  return layout_tree_object_is_within(object, stay_within);
}

static layout_object_t*
layout_tree_object_first_child(layout_tree_t* tree, layout_object_t* object) {
  layout_object_child_list_t* children = layout_tree_children(tree, object);

  return children == NULL ? NULL : children->first_child;
}

static layout_object_t*
layout_tree_object_last_child(layout_tree_t* tree, layout_object_t* object) {
  layout_object_child_list_t* children = layout_tree_children(tree, object);

  return children == NULL ? NULL : children->last_child;
}

layout_object_t*
layout_tree_object_next_in_preorder(layout_tree_t* tree,
                                    layout_object_t* object,
                                    layout_object_t* stay_within) {
  layout_object_t* child;

  if (!layout_tree_object_traversal_args_valid(tree, object, stay_within)) {
    return NULL;
  }

  child = layout_tree_object_first_child(tree, object);
  if (child != NULL) {
    return child;
  }

  return layout_tree_object_next_in_preorder_after_children(tree, object, stay_within);
}

layout_object_t*
layout_tree_object_next_in_preorder_after_children(
    layout_tree_t* tree, layout_object_t* object,
    layout_object_t* stay_within) {
  layout_object_t* current;
  layout_object_t* next;

  if (!layout_tree_object_traversal_args_valid(tree, object, stay_within)
      || object == stay_within) {
    return NULL;
  }

  current = object;
  next = current->next;

  while (next == NULL) {
    current = current->parent;
    if (current == NULL || current == stay_within) {
      return NULL;
    }

    next = current->next;
  }

  return layout_tree_object_is_recorded(tree, next) ? next : NULL;
}

layout_object_t*
layout_tree_object_previous_in_preorder(layout_tree_t* tree,
                                        layout_object_t* object,
                                        layout_object_t* stay_within) {
  layout_object_t* previous;
  layout_object_t* last_child;

  if (!layout_tree_object_traversal_args_valid(tree, object, stay_within)
      || object == stay_within) {
    return NULL;
  }

  previous = object->prev;
  if (previous == NULL) {
    return object->parent == stay_within ? stay_within : object->parent;
  }

  while ((last_child = layout_tree_object_last_child(tree, previous)) != NULL) {
    previous = last_child;
  }

  return layout_tree_object_is_recorded(tree, previous) ? previous : NULL;
}

static layout_object_t*
layout_tree_object_previous_in_postorder_before_children(
    layout_tree_t* tree, layout_object_t* object,
    layout_object_t* stay_within) {
  if (object == stay_within) {
    return NULL;
  }

  for (layout_object_t* current = object; current != NULL;
       current = current->parent) {
    layout_object_t* previous = current->prev;

    if (previous != NULL) {
      return layout_tree_object_is_recorded(tree, previous) ? previous : NULL;
    }

    if (current->parent == stay_within) {
      return NULL;
    }
  }

  return NULL;
}

layout_object_t*
layout_tree_object_previous_in_postorder(layout_tree_t* tree,
                                         layout_object_t* object,
                                         layout_object_t* stay_within) {
  layout_object_t* child;

  if (!layout_tree_object_traversal_args_valid(tree, object, stay_within)) {
    return NULL;
  }

  child = layout_tree_object_last_child(tree, object);
  if (child != NULL) {
    return child;
  }

  return layout_tree_object_previous_in_postorder_before_children(
      tree,
      object,
      stay_within);
}

lxb_status_t
layout_tree_build_fragments(layout_tree_t* tree,
                            layout_result_t* result,
                            const layout_fragment_builder_t* builder) {
  return layout_algorithm_build_fragments(tree, result, builder);
}
