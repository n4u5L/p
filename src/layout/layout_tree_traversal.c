#include "layout/layout_internal.h"

#include <stdbool.h>

#include "lexbor/css/property/const.h"

static const lxb_style_computed_t*
layout_tree_traversal_node_style(lxb_dom_node_t* node) {
  return (const lxb_style_computed_t*)((lxb_dom_element_t*)node)->computed_style;
}

bool layout_tree_traversal_node_is_display_none(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_tree_traversal_node_style(node);
  if (!style) {
    return false;
  }

  non_inherited = style->non_inherited;
  if (!non_inherited) {
    return false;
  }

  return non_inherited->display_box == LXB_CSS_DISPLAY_NONE
         || non_inherited->display_outside == LXB_CSS_DISPLAY_NONE
         || non_inherited->display_inside == LXB_CSS_DISPLAY_NONE;
}

bool layout_tree_traversal_node_is_display_contents(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_tree_traversal_node_style(node);
  if (!style) {
    return false;
  }

  non_inherited = style->non_inherited;
  if (!non_inherited) {
    return false;
  }

  return non_inherited->display_box == LXB_CSS_DISPLAY_CONTENTS
         || non_inherited->display_outside == LXB_CSS_DISPLAY_CONTENTS
         || non_inherited->display_inside == LXB_CSS_DISPLAY_CONTENTS;
}

static lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling_internal(lxb_dom_node_t* node) {
  for (lxb_dom_node_t* current = node; current != NULL; current = current->next) {
    if (!layout_tree_traversal_node_is_display_contents(current)) {
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

  while (parent && layout_tree_traversal_node_is_display_contents(parent)) {
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
  while (parent && layout_tree_traversal_node_is_display_contents(parent)) {
    sibling = layout_tree_traversal_next_layout_sibling_internal(parent->next);
    if (sibling) {
      return sibling;
    }

    parent = parent->parent;
  }

  return NULL;
}
