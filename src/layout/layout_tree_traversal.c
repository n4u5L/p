#include "layout/layout_internal.h"

#include <stdbool.h>

#include "lexbor/css/property/const.h"
#include "lexbor/dom/interfaces/element.h"

static const lxb_style_computed_t*
layout_tree_traversal_node_style(lxb_dom_node_t* node) {
  if (node == NULL || node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return NULL;
  }

  return (const lxb_style_computed_t*)
      lxb_dom_interface_element(node)->computed_style;
}

bool
layout_tree_traversal_node_is_display_none(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_tree_traversal_node_style(node);
  if (style == NULL) {
    return false;
  }

  non_inherited = style->non_inherited;
  if (non_inherited == NULL) {
    return false;
  }

  return non_inherited->display_box == LXB_CSS_DISPLAY_NONE
      || non_inherited->display_outside == LXB_CSS_DISPLAY_NONE
      || non_inherited->display_inside == LXB_CSS_DISPLAY_NONE;
}

bool
layout_tree_traversal_node_is_display_contents(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_tree_traversal_node_style(node);
  if (style == NULL) {
    return false;
  }

  non_inherited = style->non_inherited;
  if (non_inherited == NULL) {
    return false;
  }

  return non_inherited->display_box == LXB_CSS_DISPLAY_CONTENTS
      || non_inherited->display_outside == LXB_CSS_DISPLAY_CONTENTS
      || non_inherited->display_inside == LXB_CSS_DISPLAY_CONTENTS;
}

static lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling_internal(lxb_dom_node_t* node) {
  for (lxb_dom_node_t* current = node; current != NULL;
       current = layout_tree_traversal_next_sibling(current)) {
    if (!layout_tree_traversal_node_is_display_contents(current)) {
      return current;
    }

    lxb_dom_node_t* child =
        layout_tree_traversal_next_layout_sibling_internal(
            layout_tree_traversal_first_child(current));
    if (child != NULL) {
      return child;
    }
  }

  return NULL;
}

lxb_dom_node_t*
layout_tree_traversal_parent(lxb_dom_node_t* node) {
  return node == NULL ? NULL : node->parent;
}

lxb_dom_node_t*
layout_tree_traversal_layout_parent(lxb_dom_node_t* node) {
  lxb_dom_node_t* parent = layout_tree_traversal_parent(node);

  while (parent != NULL
         && layout_tree_traversal_node_is_display_contents(parent)) {
    parent = layout_tree_traversal_parent(parent);
  }

  return parent;
}

lxb_dom_node_t*
layout_tree_traversal_first_child(lxb_dom_node_t* node) {
  return node == NULL ? NULL : node->first_child;
}

lxb_dom_node_t*
layout_tree_traversal_next_sibling(lxb_dom_node_t* node) {
  return node == NULL ? NULL : node->next;
}

lxb_dom_node_t*
layout_tree_traversal_first_layout_child(lxb_dom_node_t* node) {
  return layout_tree_traversal_next_layout_sibling_internal(
      layout_tree_traversal_first_child(node));
}

lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling(lxb_dom_node_t* node) {
  lxb_dom_node_t* sibling =
      layout_tree_traversal_next_layout_sibling_internal(
          layout_tree_traversal_next_sibling(node));
  lxb_dom_node_t* parent;

  if (sibling != NULL) {
    return sibling;
  }

  parent = layout_tree_traversal_parent(node);
  while (parent != NULL
         && layout_tree_traversal_node_is_display_contents(parent)) {
    sibling = layout_tree_traversal_next_layout_sibling_internal(
        layout_tree_traversal_next_sibling(parent));
    if (sibling != NULL) {
      return sibling;
    }

    parent = layout_tree_traversal_parent(parent);
  }

  return NULL;
}
