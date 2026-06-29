#include "layout/layout_internal.h"

#include <stdbool.h>

#include "lexbor/css/property/const.h"

static lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling_internal(lxb_dom_node_t* node) {
  for (lxb_dom_node_t* current = node; current != NULL; current = current->next) {
    if (!layout_internal_node_is_display_contents(current)) {
      return current;
    }

    lxb_dom_node_t* child =
        layout_tree_traversal_next_layout_sibling_internal(current->first_child);
    if (child) {
      return child;
    }
  }

  return NULL;
}

lxb_dom_node_t*
layout_tree_traversal_layout_parent(lxb_dom_node_t* node) {
  if (!node) return NULL;
  lxb_dom_node_t* parent = node->parent;

  while (parent && layout_internal_node_is_display_contents(parent)) {
    parent = parent->parent;
  }

  return parent;
}

lxb_dom_node_t*
layout_tree_traversal_first_layout_child(lxb_dom_node_t* node) {
  return layout_tree_traversal_next_layout_sibling_internal(node->first_child);
}

lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling(lxb_dom_node_t* node) {
  lxb_dom_node_t* sibling =
      layout_tree_traversal_next_layout_sibling_internal(node->next);
  lxb_dom_node_t* parent;

  if (sibling) {
    return sibling;
  }

  parent = node->parent;
  while (parent && layout_internal_node_is_display_contents(parent)) {
    sibling = layout_tree_traversal_next_layout_sibling_internal(parent->next);
    if (sibling) {
      return sibling;
    }

    parent = parent->parent;
  }

  return NULL;
}
