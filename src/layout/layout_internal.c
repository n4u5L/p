#include "layout_internal.h"

bool layout_internal_node_is_display_none(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_internal_node_style(node);
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

bool layout_internal_node_is_display_contents(lxb_dom_node_t* node) {
  const lxb_style_computed_t* style;
  const lxb_style_computed_non_inherited_t* non_inherited;

  style = layout_internal_node_style(node);
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