#include "layout/layout_internal.h"
#include "layout/layout_algorithm.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lexbor/core/lexbor.h"
#include "lexbor/css/property/const.h"

typedef struct layout_visual_order_item {
  layout_fragment_t* fragment;
  layout_point_t offset;
  int stacking_order;
  unsigned phase;
  size_t sequence;
  layout_fragment_placement_t placement;
  layout_oof_positioned_t* oof;
  bool is_oof;
  bool visited;
} layout_visual_order_item_t;

static const layout_transform_t layout_identity_transform =
    {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

static lxb_status_t
layout_result_root_point_to_oof_fragment(layout_result_t* result,
                                         layout_oof_positioned_t* oof,
                                         layout_point_t root_point,
                                         unsigned coordinate_flags,
                                         layout_point_t* out_local);

static lxb_status_t
layout_fragment_clone_subtree_internal(layout_result_t* result,
                                       layout_fragment_t* source_fragment,
                                       layout_fragment_t** out_fragment);

static lxb_status_t
layout_result_clean_fragment_read(layout_result_t* result,
                                  layout_fragment_t* fragment);

static layout_fragment_t*
layout_result_fragment_at_object(layout_fragment_t* fragment,
                                 layout_object_t* target_object,
                                 size_t* fragment_index);

static layout_fragment_t*
layout_result_oof_fragment_at_object(layout_result_t* result,
                                     layout_object_t* target_object,
                                     size_t* fragment_index);

static layout_fragment_t*
layout_result_oof_containing_fragment(layout_result_t* result,
                                      layout_oof_positioned_t* oof);

static lxb_status_t
layout_fragment_point_from_root(layout_fragment_t* root_fragment,
                                layout_fragment_t* target_fragment,
                                layout_point_t root_point,
                                unsigned coordinate_flags,
                                layout_point_t* out_local);

static lxb_status_t
layout_fragment_local_point_to_root(layout_fragment_t* root_fragment,
                                    layout_fragment_t* target_fragment,
                                    layout_point_t local_point,
                                    unsigned coordinate_flags,
                                    layout_point_t* out_root);

static lxb_status_t
layout_result_oof_local_point_to_root(layout_result_t* result,
                                      layout_oof_positioned_t* oof,
                                      layout_point_t local_point,
                                      unsigned coordinate_flags,
                                      layout_point_t* out_root);

static size_t
layout_scene_plan_child_at_slot(layout_scene_plan_t* plan,
                                const layout_scene_node_t* parent,
                                size_t slot);

static layout_fragment_t*
layout_hit_test_fragment(layout_result_t* result, layout_fragment_t* fragment,
                         layout_point_t point_in_parent);

static layout_fragment_t*
layout_hit_test_oof_fragment(layout_result_t* result,
                             layout_oof_positioned_t* oof,
                             layout_point_t point_in_paint_parent,
                             layout_point_t paint_offset);

static void
layout_object_mark_child_needs_full_layout(layout_object_t* object);

static void
layout_object_mark_spanner_or_out_of_flow_parent_change(
    layout_object_t* object);

static lxb_css_position_type_t
layout_object_position(layout_object_t* object);

static int
layout_object_style_z_index(layout_object_t* object, bool* has_z_index);

static bool
layout_scene_transform_is_identity(layout_transform_t transform);

static double
layout_scene_object_opacity(layout_object_t* object);

void
layout_object_set_internal_type_bits(layout_object_t* object,
                                     unsigned internal_bits) {
  if (object == NULL) {
    return;
  }

  object->bitfields &= ~LAYOUT_OBJECT_INTERNAL_TEXT;
  object->bitfields |= internal_bits;
}

static bool
layout_object_establishes_absolute_cb(layout_object_t* object) {
  if (object == NULL) {
    return false;
  }

  return layout_object_can_contain_absolute_position(object);
}

void
layout_object_init_fragment_data(layout_object_t* object) {
  if (object == NULL) {
    return;
  }

  memset(&object->fragment_data, 0, sizeof(object->fragment_data));
  object->fragment_data.length = 1;
  object->fragment_data.first.flags = LAYOUT_FRAGMENT_DATA_IS_FIRST;
}

static bool
layout_result_next_fragment_index_for_object(layout_result_t* result,
                                             layout_object_t* object,
                                             uint32_t* out_index) {
  layout_fragment_index_counter_t* counters;
  size_t capacity;

  if (out_index == NULL) {
    return false;
  }

  *out_index = 0;

  if (result == NULL || result->arena == NULL || object == NULL) {
    return false;
  }

  for (size_t i = 0; i < result->fragment_index_counters_length; i++) {
    layout_fragment_index_counter_t* counter =
        &result->fragment_index_counters[i];

    if (counter->object == object) {
      if (counter->next_index == UINT32_MAX) {
        return false;
      }

      *out_index = counter->next_index;
      counter->next_index++;
      return true;
    }
  }

  if (result->fragment_index_counters_length == result->fragment_index_counters_capacity) {
    capacity = result->fragment_index_counters_capacity == 0
                   ? 8
                   : result->fragment_index_counters_capacity * 2;

    if (result->fragment_index_counters == NULL) {
      counters = lexbor_mraw_calloc(
          result->arena,
          capacity * sizeof(layout_fragment_index_counter_t));
    } else {
      counters = lexbor_mraw_realloc(
          result->arena,
          result->fragment_index_counters,
          capacity * sizeof(layout_fragment_index_counter_t));
    }

    if (counters == NULL) {
      return false;
    }

    result->fragment_index_counters = counters;
    result->fragment_index_counters_capacity = capacity;
  }

  result->fragment_index_counters[result->fragment_index_counters_length].object = object;
  result->fragment_index_counters[result->fragment_index_counters_length].next_index = 1;
  result->fragment_index_counters_length++;

  return true;
}

static uint64_t
layout_mix_identity_uint64(uint64_t value) {
  value ^= value >> 33;
  value *= 0xff51afd7ed558ccdull;
  value ^= value >> 33;
  value *= 0xc4ceb9fe1a85ec53ull;
  value ^= value >> 33;

  return value == 0 ? 1 : value;
}

static uint64_t
layout_dom_object_id(layout_t* layout, lxb_dom_node_t* node) {
  return layout_mix_identity_uint64(
      (uint64_t)(uintptr_t)node ^ (layout == NULL ? 0 : layout->identity_salt));
}

static uint64_t
layout_anonymous_object_id(layout_t* layout, layout_object_t* object) {
  return layout_mix_identity_uint64(
      (uint64_t)(uintptr_t)object ^ (layout == NULL ? 0 : layout->anonymous_identity_salt) ^ (layout == NULL ? 0 : layout->identity_salt));
}

static uint64_t
layout_result_identity_generation(layout_t* layout, layout_result_t* result) {
  return layout_mix_identity_uint64(
      (uint64_t)(uintptr_t)result ^ (layout == NULL ? 0 : layout->generation_salt) ^ (layout == NULL ? 0 : layout->identity_salt));
}

static layout_fragment_role_t
layout_fragment_default_role(layout_object_t* object,
                             layout_fragment_box_type_t box_type) {
  if (object == NULL) {
    return LAYOUT_FRAGMENT_ROLE_NORMAL;
  }

  if (layout_object_is_native_embed_host(object)) {
    return LAYOUT_FRAGMENT_ROLE_NATIVE_EMBED;
  }

  if (box_type == LAYOUT_FRAGMENT_BOX_OUT_OF_FLOW_POSITIONED) {
    return LAYOUT_FRAGMENT_ROLE_OUT_OF_FLOW;
  }

  if (layout_object_is_anonymous(object)) {
    return LAYOUT_FRAGMENT_ROLE_ANONYMOUS;
  }

  return LAYOUT_FRAGMENT_ROLE_NORMAL;
}

void
layout_object_unlink_from_siblings(layout_object_t* object) {
  if (object == NULL) {
    return;
  }

  if (object->prev != NULL) {
    object->prev->next = object->next;
  }

  if (object->next != NULL) {
    object->next->prev = object->prev;
  }

  object->prev = NULL;
  object->next = NULL;
}

static bool
layout_object_establishes_fixed_cb(layout_object_t* object) {
  if (object == NULL) {
    return false;
  }

  return layout_object_can_contain_fixed_position(object);
}

static layout_object_t*
layout_object_column_spanner_container(layout_object_t* object) {
  if (object == NULL) {
    return NULL;
  }

  for (layout_object_t* ancestor = object->parent; ancestor != NULL;
       ancestor = ancestor->parent) {
    if (layout_object_is_column_spanner_container(ancestor)) {
      return ancestor;
    }
  }

  return NULL;
}

static layout_object_t*
layout_object_next_containing_block_candidate(layout_object_t* object) {
  layout_object_t* spanner_container;

  if (object == NULL) {
    return NULL;
  }

  if (layout_object_is_column_spanner(object)) {
    spanner_container = layout_object_column_spanner_container(object);
    if (spanner_container != NULL) {
      return spanner_container;
    }
  }

  return object->parent;
}

static bool
layout_object_display_none(layout_object_t* object) {
  const lxb_style_computed_non_inherited_t* non_inherited;

  if (object == NULL || object->style == NULL) {
    return false;
  }

  non_inherited = object->style->non_inherited;
  if (non_inherited == NULL) {
    return false;
  }

  return non_inherited->display_box == LXB_CSS_DISPLAY_NONE || non_inherited->display_outside == LXB_CSS_DISPLAY_NONE || non_inherited->display_inside == LXB_CSS_DISPLAY_NONE;
}

void
layout_object_update_style_derived_bits(layout_object_t* object) {
  lxb_css_position_type_t position;

  if (object == NULL) {
    return;
  }

  position = layout_object_position(object);
  if ((object->bitfields & LAYOUT_OBJECT_INTERNAL_VIEWPORT_CONTAINING_BLOCK) != 0) {
    layout_object_set_can_contain_absolute_position(object, true);
    layout_object_set_can_contain_fixed_position(object, true);
    return;
  }

  layout_object_set_can_contain_absolute_position(
      object,
      position != LXB_CSS_POSITION_STATIC);
}

static bool
layout_object_visible(layout_object_t* object) {
  if (object == NULL || object->style == NULL || object->style->inherited == NULL) {
    return true;
  }

  return object->style->inherited->visibility == LXB_CSS_VISIBILITY_VISIBLE;
}

static bool
layout_object_hit_testable(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_POINTER_EVENTS_NONE) == 0 && layout_object_visible(object) && !layout_object_display_none(object);
}

static bool
layout_object_can_hit_test_descendants(layout_object_t* object) {
  return object != NULL && !layout_object_display_none(object);
}

static lxb_css_position_type_t
layout_object_position(layout_object_t* object) {
  if (object == NULL || object->style == NULL || object->style->non_inherited == NULL) {
    return LXB_CSS_POSITION_STATIC;
  }

  return object->style->non_inherited->position;
}

static bool
layout_object_is_absolute_positioned(layout_object_t* object) {
  return layout_object_position(object) == LXB_CSS_POSITION_ABSOLUTE;
}

static bool
layout_object_is_fixed_positioned(layout_object_t* object) {
  return layout_object_position(object) == LXB_CSS_POSITION_FIXED;
}

static bool
layout_object_is_out_of_flow_positioned(layout_object_t* object) {
  return layout_object_is_absolute_positioned(object) || layout_object_is_fixed_positioned(object);
}

static void
layout_object_mark_child_needs_full_layout(layout_object_t* object) {
  if (object == NULL) {
    return;
  }

  object->dirty_bits |= LAYOUT_DIRTY_CHILD_FULL;

  for (layout_object_t* ancestor = object->parent; ancestor != NULL;
       ancestor = ancestor->parent) {
    ancestor->dirty_bits |= LAYOUT_DIRTY_CHILD_FULL;
  }
}

static void
layout_object_mark_spanner_or_out_of_flow_parent_change(layout_object_t* object) {
  layout_object_t* containing_block = NULL;

  if (object == NULL || (!layout_object_is_out_of_flow_positioned(object) && !layout_object_is_column_spanner(object))) {
    return;
  }

  if (layout_object_is_column_spanner(object)) {
    containing_block =
        layout_object_containing_block_for_column_spanner(object);
  } else if (layout_object_is_fixed_positioned(object)) {
    containing_block = layout_object_containing_block_for_fixed(object);
  } else {
    containing_block = layout_object_containing_block_for_absolute(object);
  }

  if (containing_block == NULL) {
    return;
  }

  for (layout_object_t* ancestor = object->parent; ancestor != NULL && ancestor != containing_block; ancestor = ancestor->parent) {
    ancestor->dirty_bits |= LAYOUT_DIRTY_CHILD_FULL;
  }

  layout_object_mark_child_needs_full_layout(containing_block);
}

static bool
layout_object_hit_clip_axes(layout_object_t* object,
                            layout_overflow_clip_t* out_clip) {
  layout_overflow_clip_t clip;

  if (out_clip == NULL) {
    return false;
  }

  clip = layout_object_overflow_clip(object);
  if (layout_object_has_clip_path(object)) {
    clip = (layout_overflow_clip_t)(clip | LAYOUT_OVERFLOW_CLIP_BOTH);
  }

  *out_clip = clip;
  return clip != LAYOUT_OVERFLOW_CLIP_NONE;
}

static void
layout_object_release_styles(layout_t* layout) {
  size_t length;

  if (layout == NULL || layout->objects == NULL) {
    return;
  }

  length = lexbor_dobject_allocated(layout->objects);
  for (size_t i = 0; i < length; i++) {
    layout_object_t* object = lexbor_dobject_by_absolute_position(
        layout->objects,
        i);

    if (object != NULL && object->style != NULL) {
      if (object->node != NULL && object->node->layout_object == object) {
        object->node->layout_object = NULL;
      }
      lxb_style_computed_unref((lxb_style_computed_t*)object->style);
      object->style = NULL;
      object->node = NULL;
      object->parent = NULL;
      object->prev = NULL;
      object->next = NULL;
    }
  }
}

void
layout_tree_prepare_dom_node_for_attach(lxb_dom_node_t* node) {
  layout_object_t* object = layout_dom_node_layout_object(node);

  if (object == NULL) {
    return;
  }

  object->parent = NULL;
  object->prev = NULL;
  object->next = NULL;
  object->bitfields &= ~(LAYOUT_OBJECT_INTERNAL_BLOCK | LAYOUT_OBJECT_INTERNAL_ANONYMOUS_BLOCK | LAYOUT_OBJECT_INTERNAL_VIEWPORT_CONTAINING_BLOCK | LAYOUT_OBJECT_CAN_CONTAIN_ABSOLUTE_POSITION | LAYOUT_OBJECT_CAN_CONTAIN_FIXED_POSITION);
  layout_object_update_style_derived_bits(object);
}

static bool
layout_rect_contains(layout_rect_t rect, layout_point_t point,
                     layout_overflow_clip_t clip) {
  if ((clip & LAYOUT_OVERFLOW_CLIP_X) != 0 && (point.x < 0.0 || point.x > rect.width)) {
    return false;
  }

  if ((clip & LAYOUT_OVERFLOW_CLIP_Y) != 0 && (point.y < 0.0 || point.y > rect.height)) {
    return false;
  }

  return true;
}

static layout_size_t
layout_fragment_size_unsafe(layout_fragment_t* fragment) {
  layout_size_t size = {0.0, 0.0};

  return fragment == NULL ? size : fragment->size;
}

static layout_rect_t
layout_fragment_local_rect_unsafe(layout_fragment_t* fragment) {
  layout_rect_t rect = {0.0, 0.0, 0.0, 0.0};

  if (fragment != NULL) {
    rect.width = fragment->size.width;
    rect.height = fragment->size.height;
  }

  return rect;
}

static double
layout_non_negative(double value) {
  return value < 0.0 ? 0.0 : value;
}

static layout_rect_t
layout_fragment_content_rect_unsafe(layout_fragment_t* fragment) {
  layout_rect_t rect = {0.0, 0.0, 0.0, 0.0};
  double horizontal_edges;
  double vertical_edges;

  if (fragment == NULL) {
    return rect;
  }

  rect.x = fragment->border.left + fragment->padding.left;
  rect.y = fragment->border.top + fragment->padding.top;
  horizontal_edges = rect.x + fragment->padding.right + fragment->border.right;
  vertical_edges = rect.y + fragment->padding.bottom + fragment->border.bottom;
  rect.width = layout_non_negative(fragment->size.width - horizontal_edges);
  rect.height = layout_non_negative(fragment->size.height - vertical_edges);

  return rect;
}

static unsigned
layout_box_sides_from_edges(layout_box_edges_t edges) {
  unsigned sides = LAYOUT_BOX_SIDE_NONE;

  if (edges.top > 0.0) {
    sides |= LAYOUT_BOX_SIDE_TOP;
  }
  if (edges.right > 0.0) {
    sides |= LAYOUT_BOX_SIDE_RIGHT;
  }
  if (edges.bottom > 0.0) {
    sides |= LAYOUT_BOX_SIDE_BOTTOM;
  }
  if (edges.left > 0.0) {
    sides |= LAYOUT_BOX_SIDE_LEFT;
  }

  return sides;
}

static layout_point_t
layout_fragment_link_offset_unsafe(layout_fragment_link_t* link) {
  layout_point_t offset = {0.0, 0.0};

  return link == NULL ? offset : link->offset;
}

static int
layout_fragment_link_stacking_order_unsafe(layout_fragment_link_t* link) {
  return link == NULL ? 0 : link->stacking_order;
}

static bool
layout_fragment_self_contains(layout_fragment_t* fragment,
                              layout_point_t local_point) {
  return local_point.x >= 0.0 && local_point.y >= 0.0 && local_point.x <= fragment->size.width && local_point.y <= fragment->size.height;
}

static bool
layout_fragment_may_intersect(layout_fragment_t* fragment,
                              layout_point_t local_point,
                              bool has_external_children) {
  if (fragment == NULL) {
    return false;
  }

  /*
   * Minimal PhysicalBoxFragment::MayIntersect equivalent.  Without ink
   * overflow data, overflow-visible descendants may still be hit outside the
   * border box, but empty leaf fragments cannot produce a hit.
   */
  if ((fragment->size.width <= 0.0 || fragment->size.height <= 0.0) && fragment->children_length == 0 && !has_external_children) {
    return false;
  }

  if (layout_fragment_self_contains(fragment, local_point)) {
    return true;
  }

  return fragment->children_length != 0 || has_external_children;
}

static bool
layout_fragment_ancestor_to_local(layout_fragment_t* fragment,
                                  layout_point_t ancestor_point,
                                  unsigned coordinate_flags,
                                  layout_point_t* out_local) {
  layout_transform_t inverse;
  layout_point_t point = ancestor_point;

  if ((coordinate_flags & LAYOUT_COORDINATE_IGNORE_TRANSFORMS) != 0) {
    *out_local = point;
    return true;
  }

  if (!layout_transform_invert(&fragment->transform, &inverse)) {
    return false;
  }

  *out_local = layout_transform_map_point(&inverse, point);
  return true;
}

static layout_point_t
layout_fragment_local_to_ancestor(layout_fragment_t* fragment,
                                  layout_point_t local_point,
                                  unsigned coordinate_flags) {
  if (fragment == NULL || (coordinate_flags & LAYOUT_COORDINATE_IGNORE_TRANSFORMS) != 0) {
    return local_point;
  }

  return layout_transform_map_point(&fragment->transform, local_point);
}

static bool
layout_fragment_find_path(layout_fragment_t* current,
                          layout_fragment_t* target,
                          const layout_fragment_link_t** links,
                          size_t* length,
                          size_t capacity) {
  if (current == NULL || target == NULL || length == NULL) {
    return false;
  }

  if (current == target) {
    return true;
  }

  if (*length == capacity) {
    return false;
  }

  for (size_t i = 0; i < current->children_length; i++) {
    links[*length] = &current->children[i];
    (*length)++;

    if (layout_fragment_find_path(current->children[i].fragment, target, links, length, capacity)) {
      return true;
    }

    (*length)--;
  }

  return false;
}

static layout_oof_positioned_t*
layout_result_find_oof_for_fragment(layout_result_t* result,
                                    layout_fragment_t* target) {
  if (result == NULL || target == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (result->oofs[i].fragment == target) {
      return &result->oofs[i];
    }
  }

  return NULL;
}

static layout_fragment_t*
layout_result_oof_containing_fragment(layout_result_t* result,
                                      layout_oof_positioned_t* oof) {
  layout_fragment_t* containing_fragment;
  size_t fragment_index = 0;

  if (result == NULL || oof == NULL) {
    return NULL;
  }

  if (oof->containing_fragment != NULL) {
    return oof->containing_fragment;
  }

  containing_fragment =
      layout_result_fragment_at_object(result->root_fragment,
                                       oof->containing_block,
                                       &fragment_index);
  if (containing_fragment != NULL) {
    return containing_fragment;
  }

  fragment_index = 0;
  return layout_result_oof_fragment_at_object(result, oof->containing_block, &fragment_index);
}

static layout_fragment_t*
layout_result_fragment_at_object(layout_fragment_t* fragment,
                                 layout_object_t* target_object,
                                 size_t* fragment_index) {
  layout_fragment_t* found;

  if (fragment == NULL || target_object == NULL || fragment_index == NULL) {
    return NULL;
  }

  if (fragment->object == target_object) {
    if (*fragment_index == 0) {
      return fragment;
    }

    (*fragment_index)--;
  }

  for (size_t i = 0; i < fragment->children_length; i++) {
    found = layout_result_fragment_at_object(fragment->children[i].fragment,
                                             target_object,
                                             fragment_index);
    if (found != NULL) {
      return found;
    }
  }

  return NULL;
}

static layout_fragment_t*
layout_result_oof_fragment_at_object(layout_result_t* result,
                                     layout_object_t* target_object,
                                     size_t* fragment_index) {
  if (result == NULL || target_object == NULL || fragment_index == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (result->oofs[i].fragment != NULL && result->oofs[i].fragment->object == target_object) {
      if (*fragment_index == 0) {
        return result->oofs[i].fragment;
      }

      (*fragment_index)--;
    }
  }

  return NULL;
}

static bool
layout_fragment_tree_can_traverse(layout_fragment_t* fragment) {
  if (fragment == NULL || fragment->object == NULL) {
    return false;
  }

  if (!layout_fragment_children_valid(fragment)) {
    return false;
  }

  if (!layout_object_can_traverse_physical_fragments(fragment->object)) {
    return false;
  }

  for (size_t i = 0; i < fragment->children_length; i++) {
    if (!layout_fragment_tree_can_traverse(fragment->children[i].fragment)) {
      return false;
    }
  }

  return true;
}

static bool
layout_fragment_tree_contains(layout_fragment_t* current,
                              layout_fragment_t* target) {
  if (current == NULL || target == NULL) {
    return false;
  }

  if (current == target) {
    return true;
  }

  if (!layout_fragment_children_valid(current)) {
    return false;
  }

  for (size_t i = 0; i < current->children_length; i++) {
    if (layout_fragment_tree_contains(current->children[i].fragment,
                                      target)) {
      return true;
    }
  }

  return false;
}

static bool
layout_fragment_tree_needs_layout(layout_fragment_t* fragment) {
  if (fragment == NULL) {
    return true;
  }

  if (!layout_fragment_children_valid(fragment)) {
    return true;
  }

  if (layout_object_subtree_needs_layout(fragment->object)) {
    return true;
  }

  for (size_t i = 0; i < fragment->children_length; i++) {
    if (layout_fragment_tree_needs_layout(fragment->children[i].fragment)) {
      return true;
    }
  }

  return false;
}

static bool
layout_result_out_of_flow_needs_layout(layout_result_t* result) {
  if (result == NULL) {
    return true;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (layout_object_subtree_needs_layout(result->oofs[i].object) || layout_object_subtree_needs_layout(result->oofs[i].containing_block) || layout_fragment_tree_needs_layout(result->oofs[i].fragment)) {
      return true;
    }
  }

  return false;
}

static bool
layout_fragment_path_can_traverse(layout_fragment_t* root,
                                  layout_fragment_t* target) {
  const layout_fragment_link_t* links[128];
  size_t length = 0;

  if (root == NULL || target == NULL || !layout_fragment_find_path(root, target, links, &length, sizeof(links) / sizeof(links[0]))) {
    return false;
  }

  if (!layout_object_can_traverse_physical_fragments(root->object)) {
    return false;
  }

  if (!layout_fragment_children_valid(root)) {
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    if (!layout_fragment_children_valid(links[i]->fragment)) {
      return false;
    }

    if (!layout_object_can_traverse_physical_fragments(
            links[i]->fragment->object)) {
      return false;
    }
  }

  return true;
}

static bool
layout_fragment_establishes_stacking_context(layout_fragment_t* fragment) {
  layout_object_t* object;
  bool has_z_index = false;

  if (fragment == NULL) {
    return false;
  }

  object = fragment->object;
  if (object == NULL) {
    return false;
  }

  (void)layout_object_style_z_index(object, &has_z_index);
  if (has_z_index && layout_object_position(object) != LXB_CSS_POSITION_STATIC) {
    return true;
  }

  if (layout_object_is_fixed_positioned(object)) {
    return true;
  }

  if (layout_scene_object_opacity(object) < 1.0) {
    return true;
  }

  return !layout_scene_transform_is_identity(fragment->transform);
}

static int
layout_fragment_paint_stacking_order(layout_fragment_t* fragment,
                                     int fallback_stacking_order) {
  layout_object_t* object;
  bool has_z_index = false;
  int z_index;

  if (fragment == NULL) {
    return fallback_stacking_order;
  }

  object = fragment->object;
  z_index = layout_object_style_z_index(object, &has_z_index);
  if (has_z_index && layout_object_position(object) != LXB_CSS_POSITION_STATIC) {
    return z_index;
  }

  return fallback_stacking_order;
}

static unsigned
layout_visual_order_placement_rank(layout_fragment_placement_t placement,
                                   bool is_oof) {
  if (placement == LAYOUT_FRAGMENT_PLACEMENT_OVERLAY) {
    return 4u;
  }

  if (placement == LAYOUT_FRAGMENT_PLACEMENT_SCROLL_CONTENTS) {
    return 3u;
  }

  if (is_oof || placement == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW) {
    return 2u;
  }

  if (placement == LAYOUT_FRAGMENT_PLACEMENT_FLOAT) {
    return 1u;
  }

  return 0u;
}

static lxb_status_t
layout_visual_order_fragment_local_point_to_root(layout_result_t* result,
                                                 layout_fragment_t* fragment,
                                                 layout_point_t local_point,
                                                 layout_point_t* out_root) {
  layout_oof_positioned_t* oof;

  if (result == NULL || fragment == NULL || out_root == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  oof = layout_result_find_oof_for_fragment(result, fragment);
  if (oof != NULL) {
    return layout_result_oof_local_point_to_root(
        result,
        oof,
        local_point,
        LAYOUT_COORDINATE_APPLY_TRANSFORMS,
        out_root);
  }

  if (!layout_fragment_path_can_traverse(result->root_fragment, fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  return layout_fragment_local_point_to_root(
      result->root_fragment,
      fragment,
      local_point,
      LAYOUT_COORDINATE_APPLY_TRANSFORMS,
      out_root);
}

static lxb_status_t
layout_visual_order_root_point_to_fragment(layout_result_t* result,
                                           layout_fragment_t* fragment,
                                           layout_point_t root_point,
                                           layout_point_t* out_local) {
  layout_oof_positioned_t* oof;

  if (result == NULL || fragment == NULL || out_local == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  oof = layout_result_find_oof_for_fragment(result, fragment);
  if (oof != NULL) {
    return layout_result_root_point_to_oof_fragment(
        result,
        oof,
        root_point,
        LAYOUT_COORDINATE_APPLY_TRANSFORMS,
        out_local);
  }

  if (!layout_fragment_path_can_traverse(result->root_fragment, fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  return layout_fragment_point_from_root(
      result->root_fragment,
      fragment,
      root_point,
      LAYOUT_COORDINATE_APPLY_TRANSFORMS,
      out_local);
}

static layout_fragment_t*
layout_visual_order_fragment_for_object(layout_result_t* result,
                                        layout_object_t* object) {
  layout_fragment_t* fragment;
  size_t fragment_index = 0;

  if (result == NULL || object == NULL) {
    return NULL;
  }

  fragment = layout_result_fragment_at_object(result->root_fragment, object, &fragment_index);
  if (fragment != NULL) {
    return fragment;
  }

  fragment_index = 0;
  return layout_result_oof_fragment_at_object(result, object, &fragment_index);
}

static layout_fragment_t*
layout_visual_order_oof_parent_fragment(layout_result_t* result,
                                        layout_oof_positioned_t* oof) {
  layout_fragment_t* containing_fragment;

  if (result == NULL || oof == NULL) {
    return NULL;
  }

  containing_fragment = layout_result_oof_containing_fragment(result, oof);
  if (containing_fragment == NULL) {
    return NULL;
  }

  /*
   * Geometry is resolved against the containing block, but painting is scoped
   * by the nearest ancestor stacking context.  Keep the registry's geometric
   * containing fragment intact and only choose a paint parent here.
   */
  for (layout_object_t* ancestor = oof->object == NULL
                                       ? NULL
                                       : oof->object->parent;
       ancestor != NULL;
       ancestor = ancestor->parent) {
    layout_fragment_t* fragment =
        layout_visual_order_fragment_for_object(result, ancestor);

    if (fragment != NULL && layout_fragment_establishes_stacking_context(fragment)) {
      return fragment;
    }
  }

  return containing_fragment;
}

static lxb_status_t
layout_visual_order_oof_offset_from_parent(layout_result_t* result,
                                           layout_oof_positioned_t* oof,
                                           layout_fragment_t* paint_parent,
                                           layout_point_t* out_offset) {
  layout_fragment_t* containing_fragment;
  layout_point_t root_point;
  lxb_status_t status;

  if (result == NULL || oof == NULL || paint_parent == NULL || out_offset == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  containing_fragment = layout_result_oof_containing_fragment(result, oof);
  if (containing_fragment == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  if (paint_parent == containing_fragment) {
    *out_offset = oof->offset;
    return LXB_STATUS_OK;
  }

  status = layout_visual_order_fragment_local_point_to_root(
      result,
      containing_fragment,
      oof->offset,
      &root_point);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  return layout_visual_order_root_point_to_fragment(result, paint_parent, root_point, out_offset);
}

static unsigned
layout_visual_order_item_phase(const layout_visual_order_item_t* item) {
  layout_object_t* object;

  if (item == NULL) {
    return 0u;
  }

  if (item->placement == LAYOUT_FRAGMENT_PLACEMENT_OVERLAY) {
    return 5u;
  }

  if (item->stacking_order < 0) {
    return 0u;
  }

  if (item->stacking_order > 0) {
    return 4u;
  }

  if (item->placement == LAYOUT_FRAGMENT_PLACEMENT_FLOAT) {
    return 2u;
  }

  object = item->fragment == NULL ? NULL : item->fragment->object;
  if (item->is_oof || item->placement == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW || (object != NULL && layout_object_position(object) != LXB_CSS_POSITION_STATIC) || layout_fragment_establishes_stacking_context(item->fragment)) {
    return 3u;
  }

  return 1u;
}

static int
layout_visual_order_item_compare(const layout_visual_order_item_t* left,
                                 const layout_visual_order_item_t* right) {
  unsigned left_rank;
  unsigned right_rank;

  if (left == NULL || right == NULL) {
    return left == right ? 0 : (left == NULL ? -1 : 1);
  }

  if (left->phase != right->phase) {
    return left->phase < right->phase ? -1 : 1;
  }

  if (left->stacking_order != right->stacking_order) {
    return left->stacking_order < right->stacking_order ? -1 : 1;
  }

  left_rank =
      layout_visual_order_placement_rank(left->placement, left->is_oof);
  right_rank =
      layout_visual_order_placement_rank(right->placement, right->is_oof);
  if (left_rank != right_rank) {
    return left_rank < right_rank ? -1 : 1;
  }

  if (left->sequence != right->sequence) {
    return left->sequence < right->sequence ? -1 : 1;
  }

  return 0;
}

static bool
layout_visual_order_item_paints_after(
    const layout_visual_order_item_t* candidate,
    const layout_visual_order_item_t* current) {
  if (candidate == NULL) {
    return false;
  }

  if (current == NULL) {
    return true;
  }

  return layout_visual_order_item_compare(candidate, current) > 0;
}

static bool
layout_visual_order_item_paints_before(
    const layout_visual_order_item_t* candidate,
    const layout_visual_order_item_t* current) {
  if (candidate == NULL) {
    return false;
  }

  if (current == NULL) {
    return true;
  }

  return layout_visual_order_item_compare(candidate, current) < 0;
}

static size_t
layout_visual_order_out_of_flow_count(layout_result_t* result,
                                      layout_fragment_t* fragment) {
  size_t count = 0;

  if (result == NULL || fragment == NULL) {
    return 0;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (layout_visual_order_oof_parent_fragment(result, &result->oofs[i]) == fragment) {
      count++;
    }
  }

  return count;
}

static lxb_status_t
layout_visual_order_collect_items(layout_result_t* result,
                                  layout_fragment_t* fragment,
                                  layout_visual_order_item_t* items,
                                  size_t capacity,
                                  size_t* out_length) {
  size_t index = 0;
  size_t child_count;

  if (out_length != NULL) {
    *out_length = 0;
  }

  if (fragment == NULL || items == NULL || out_length == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  child_count = layout_fragment_child_count(fragment);
  for (size_t i = 0; i < child_count && index < capacity; i++) {
    layout_fragment_link_t* link = layout_fragment_child_link_at(fragment,
                                                                 i);

    items[index].fragment = link->fragment;
    items[index].offset = link->offset;
    items[index].stacking_order =
        layout_fragment_paint_stacking_order(link->fragment,
                                             link->stacking_order);
    items[index].sequence = link->sequence;
    items[index].placement = link->placement;
    items[index].oof = NULL;
    items[index].is_oof = false;
    items[index].phase = layout_visual_order_item_phase(&items[index]);
    index++;
  }

  if (result == NULL) {
    *out_length = index;
    return LXB_STATUS_OK;
  }

  for (size_t i = 0; i < result->oofs_length && index < capacity; i++) {
    layout_oof_positioned_t* oof = &result->oofs[i];
    layout_fragment_t* paint_parent =
        layout_visual_order_oof_parent_fragment(result, oof);
    layout_point_t paint_offset;
    lxb_status_t status;

    if (paint_parent != fragment) {
      continue;
    }

    status = layout_visual_order_oof_offset_from_parent(
        result,
        oof,
        paint_parent,
        &paint_offset);
    if (status != LXB_STATUS_OK) {
      return status;
    }

    items[index].fragment = oof->fragment;
    items[index].offset = paint_offset;
    items[index].stacking_order =
        layout_fragment_paint_stacking_order(oof->fragment,
                                             oof->stacking_order);
    items[index].sequence = i;
    items[index].placement = oof->placement;
    items[index].oof = oof;
    items[index].is_oof = true;
    items[index].phase = layout_visual_order_item_phase(&items[index]);
    index++;
  }

  *out_length = index;
  return LXB_STATUS_OK;
}

static layout_visual_order_item_t*
layout_visual_order_next_item(layout_visual_order_item_t* items, size_t length,
                              bool reverse) {
  layout_visual_order_item_t* candidate = NULL;

  if (items == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < length; i++) {
    if (items[i].visited) {
      continue;
    }

    if (reverse
            ? layout_visual_order_item_paints_after(&items[i], candidate)
            : layout_visual_order_item_paints_before(&items[i], candidate)) {
      candidate = &items[i];
    }
  }

  if (candidate != NULL) {
    candidate->visited = true;
  }

  return candidate;
}

static layout_fragment_t*
layout_hit_test_visual_order_item(layout_result_t* result,
                                  layout_visual_order_item_t* item,
                                  layout_point_t local_point) {
  if (item == NULL) {
    return NULL;
  }

  if (item->is_oof) {
    return layout_hit_test_oof_fragment(result, item->oof, local_point, item->offset);
  }

  local_point.x -= item->offset.x;
  local_point.y -= item->offset.y;

  return layout_hit_test_fragment(result, item->fragment, local_point);
}

static layout_fragment_t*
layout_hit_test_fragment_local(layout_result_t* result,
                               layout_fragment_t* fragment,
                               layout_point_t local_point) {
  layout_overflow_clip_t clip;
  size_t oof_items = 0;
  size_t items_length;
  layout_visual_order_item_t* items = NULL;

  if (fragment == NULL || fragment->object == NULL) {
    return NULL;
  }

  if (layout_fragment_hidden_for_paint(fragment) || !layout_object_can_hit_test_descendants(fragment->object)) {
    return NULL;
  }

  if (layout_object_hit_clip_axes(fragment->object, &clip) && !layout_rect_contains(layout_fragment_local_rect_unsafe(fragment),
                                                                                    local_point,
                                                                                    clip)) {
    return NULL;
  }

  oof_items = layout_visual_order_out_of_flow_count(result, fragment);

  if (!layout_fragment_may_intersect(fragment, local_point, oof_items != 0)) {
    return NULL;
  }

  items_length = fragment->children_length + oof_items;
  if (items_length != 0) {
    lxb_status_t status;

    items = lexbor_calloc(items_length,
                          sizeof(layout_visual_order_item_t));
    if (items == NULL) {
      return NULL;
    }

    status = layout_visual_order_collect_items(
        result,
        fragment,
        items,
        items_length,
        &items_length);
    if (status != LXB_STATUS_OK) {
      lexbor_free(items);
      return NULL;
    }
  }

  for (size_t visited_count = 0; visited_count < items_length;
       visited_count++) {
    layout_visual_order_item_t* item;
    layout_fragment_t* hit;

    item = layout_visual_order_next_item(items, items_length, true);
    if (item == NULL) {
      break;
    }

    hit = layout_hit_test_visual_order_item(result, item, local_point);
    if (hit != NULL) {
      lexbor_free(items);
      return hit;
    }
  }

  lexbor_free(items);

  if (layout_object_hit_testable(fragment->object) && !layout_fragment_opaque(fragment) && layout_fragment_self_contains(fragment, local_point)) {
    return fragment;
  }

  return NULL;
}

static layout_fragment_t*
layout_hit_test_fragment(layout_result_t* result, layout_fragment_t* fragment,
                         layout_point_t point_in_parent) {
  layout_point_t local_point;
  layout_object_t* object;

  if (fragment == NULL || fragment->object == NULL) {
    return NULL;
  }

  if (layout_fragment_hidden_for_paint(fragment)) {
    return NULL;
  }

  object = fragment->object;
  if (!layout_object_can_hit_test_descendants(object)) {
    return NULL;
  }

  if (!layout_fragment_ancestor_to_local(fragment, point_in_parent, LAYOUT_COORDINATE_APPLY_TRANSFORMS, &local_point)) {
    return NULL;
  }

  return layout_hit_test_fragment_local(result, fragment, local_point);
}

static layout_fragment_t*
layout_hit_test_oof_fragment(layout_result_t* result,
                             layout_oof_positioned_t* oof,
                             layout_point_t point_in_paint_parent,
                             layout_point_t paint_offset) {
  layout_point_t point = point_in_paint_parent;

  if (oof == NULL || oof->fragment == NULL) {
    return NULL;
  }

  (void)result;

  point.x -= paint_offset.x;
  point.y -= paint_offset.y;

  if (!layout_fragment_ancestor_to_local(oof->fragment, point, LAYOUT_COORDINATE_APPLY_TRANSFORMS, &point)) {
    return NULL;
  }

  return layout_hit_test_fragment_local(result, oof->fragment, point);
}

layout_t*
layout_create(void) {
  return lexbor_calloc(1, sizeof(layout_t));
}

lxb_status_t
layout_init(layout_t* layout) {
  lxb_status_t status;

  if (layout == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  layout->objects = lexbor_dobject_create();
  layout->blocks = lexbor_dobject_create();

  if (layout->objects == NULL || layout->blocks == NULL) {
    layout_destroy(layout, false);
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  status = lexbor_dobject_init(layout->objects, LAYOUT_OBJECT_CHUNK, sizeof(layout_object_t));
  if (status != LXB_STATUS_OK) {
    layout_destroy(layout, false);
    return status;
  }

  status = lexbor_dobject_init(layout->blocks, LAYOUT_BLOCK_CHUNK, sizeof(layout_block_t));
  if (status != LXB_STATUS_OK) {
    layout_destroy(layout, false);
    return status;
  }

  layout->identity_salt = layout_mix_identity_uint64(
      (uint64_t)(uintptr_t)layout ^ LAYOUT_IDENTITY_SALT);
  layout->anonymous_identity_salt = layout_mix_identity_uint64(
      layout->identity_salt ^ LAYOUT_ANONYMOUS_IDENTITY_SALT);
  layout->generation_salt = layout_mix_identity_uint64(
      layout->identity_salt ^ LAYOUT_GENERATION_SALT);

  return LXB_STATUS_OK;
}

layout_t*
layout_destroy(layout_t* layout, bool destroy_self) {
  if (layout == NULL) {
    return NULL;
  }

  layout_object_release_styles(layout);

  layout->blocks = lexbor_dobject_destroy(layout->blocks, true);
  layout->objects = lexbor_dobject_destroy(layout->objects, true);

  if (destroy_self) {
    return lexbor_free(layout);
  }

  memset(layout, 0, sizeof(layout_t));
  return layout;
}

void layout_clean(layout_t* layout) {
  if (layout == NULL) {
    return;
  }

  layout_object_release_styles(layout);

  if (layout->objects != NULL) {
    lexbor_dobject_clean(layout->objects);
  }

  if (layout->blocks != NULL) {
    lexbor_dobject_clean(layout->blocks);
  }

  layout->anonymous_identity_salt = layout_mix_identity_uint64(
      layout->anonymous_identity_salt ^ (uint64_t)(uintptr_t)layout ^ LAYOUT_ANONYMOUS_IDENTITY_SALT);
}

layout_result_t*
layout_result_create(layout_t* layout) {
  layout_result_t* result;

  if (layout == NULL) {
    return NULL;
  }

  result = lexbor_calloc(1, sizeof(layout_result_t));
  if (result == NULL) {
    return NULL;
  }

  if (layout_result_init(result, layout) != LXB_STATUS_OK) {
    return layout_result_destroy(result, true);
  }

  return result;
}

lxb_status_t
layout_result_init(layout_result_t* result, layout_t* layout) {
  lxb_status_t status;

  if (result == NULL || layout == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(result, 0, sizeof(layout_result_t));
  result->layout = layout;
  result->generation = layout_result_identity_generation(layout, result);
  result->arena = lexbor_mraw_create();
  result->fragments = lexbor_dobject_create();

  if (result->arena == NULL || result->fragments == NULL) {
    layout_result_destroy(result, false);
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  status = lexbor_mraw_init(result->arena, LAYOUT_ARENA_CHUNK);
  if (status != LXB_STATUS_OK) {
    layout_result_destroy(result, false);
    return status;
  }

  status = lexbor_dobject_init(result->fragments, LAYOUT_FRAGMENT_CHUNK, sizeof(layout_fragment_t));
  if (status != LXB_STATUS_OK) {
    layout_result_destroy(result, false);
    return status;
  }

  return LXB_STATUS_OK;
}

layout_result_t*
layout_result_destroy(layout_result_t* result, bool destroy_self) {
  if (result == NULL) {
    return NULL;
  }

  result->fragments = lexbor_dobject_destroy(result->fragments, true);
  result->arena = lexbor_mraw_destroy(result->arena, true);

  if (destroy_self) {
    return lexbor_free(result);
  }

  memset(result, 0, sizeof(layout_result_t));
  return result;
}

void layout_result_clean(layout_result_t* result) {
  if (result == NULL) {
    return;
  }

  if (result->arena != NULL) {
    lexbor_mraw_clean(result->arena);
  }

  if (result->fragments != NULL) {
    lexbor_dobject_clean(result->fragments);
  }

  result->root_fragment = NULL;
  result->oofs = NULL;
  result->oofs_length = 0;
  result->oofs_capacity = 0;
  result->fragment_index_counters = NULL;
  result->fragment_index_counters_length = 0;
  result->fragment_index_counters_capacity = 0;
  result->generation = layout_result_identity_generation(result->layout,
                                                         result);
  result->frozen = false;
}

layout_t*
layout_result_layout(layout_result_t* result) {
  return result == NULL ? NULL : result->layout;
}

lxb_status_t
layout_result_freeze(layout_result_t* result) {
  if (result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (result->root_fragment == NULL) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  result->frozen = true;
  return LXB_STATUS_OK;
}

bool layout_result_is_frozen(layout_result_t* result) {
  return result != NULL && result->frozen;
}

uint64_t
layout_result_generation(layout_result_t* result) {
  return result == NULL ? 0 : result->generation;
}

lxb_status_t
layout_result_set_root_fragment(layout_result_t* result,
                                layout_fragment_t* root_fragment) {
  if (result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (root_fragment != NULL && root_fragment->result != result) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  result->root_fragment = root_fragment;
  return LXB_STATUS_OK;
}

layout_fragment_t*
layout_result_root_fragment(layout_result_t* result) {
  return result == NULL ? NULL : result->root_fragment;
}

lxb_status_t
layout_result_add_out_of_flow_positioned(layout_result_t* result,
                                         layout_object_t* object,
                                         layout_object_t* containing_block,
                                         layout_fragment_t* fragment,
                                         layout_point_t offset,
                                         bool fixed_position) {
  return layout_result_add_out_of_flow_positioned_in_fragment(
      result,
      object,
      containing_block,
      NULL,
      fragment,
      offset,
      fixed_position);
}

lxb_status_t
layout_result_add_out_of_flow_positioned_in_fragment(
    layout_result_t* result,
    layout_object_t* object,
    layout_object_t* containing_block,
    layout_fragment_t* containing_fragment,
    layout_fragment_t* fragment,
    layout_point_t offset,
    bool fixed_position) {
  layout_oof_positioned_t* oofs;
  size_t capacity;

  if (result == NULL || object == NULL || containing_block == NULL || fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (fragment->result != result || fragment->object != object) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (containing_fragment != NULL && (containing_fragment->result != result || containing_fragment->object != containing_block)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (fixed_position) {
    if (layout_object_containing_block_for_fixed(object) != containing_block) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }
  } else if (layout_object_containing_block_for_absolute(object) != containing_block) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (result->oofs_length == result->oofs_capacity) {
    capacity = result->oofs_capacity == 0 ? 4
                                          : result->oofs_capacity * 2;

    if (result->oofs == NULL) {
      oofs = lexbor_mraw_calloc(result->arena,
                                capacity * sizeof(layout_oof_positioned_t));
    } else {
      oofs = lexbor_mraw_realloc(result->arena, result->oofs, capacity * sizeof(layout_oof_positioned_t));
    }

    if (oofs == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    result->oofs = oofs;
    result->oofs_capacity = capacity;
  }

  result->oofs[result->oofs_length].object = object;
  result->oofs[result->oofs_length].containing_block = containing_block;
  result->oofs[result->oofs_length].containing_fragment =
      containing_fragment;
  result->oofs[result->oofs_length].fragment = fragment;
  result->oofs[result->oofs_length].offset = offset;
  result->oofs[result->oofs_length].stacking_order = fragment->stacking_order;
  result->oofs[result->oofs_length].placement =
      LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW;
  result->oofs[result->oofs_length].fixed_position = fixed_position;
  result->oofs_length++;

  return LXB_STATUS_OK;
}

size_t
layout_result_out_of_flow_positioned_count(layout_result_t* result) {
  return result == NULL ? 0 : result->oofs_length;
}

layout_oof_positioned_t*
layout_result_out_of_flow_positioned_at(layout_result_t* result,
                                        size_t index) {
  if (result == NULL || index >= result->oofs_length) {
    return NULL;
  }

  return &result->oofs[index];
}

layout_object_t*
layout_oof_positioned_object(layout_oof_positioned_t* oof) {
  return oof == NULL ? NULL : oof->object;
}

layout_object_t*
layout_oof_positioned_containing_block(layout_oof_positioned_t* oof) {
  return oof == NULL ? NULL : oof->containing_block;
}

layout_fragment_t*
layout_oof_positioned_containing_fragment(layout_oof_positioned_t* oof) {
  return oof == NULL ? NULL : oof->containing_fragment;
}

layout_fragment_t*
layout_oof_positioned_fragment(layout_oof_positioned_t* oof) {
  return oof == NULL ? NULL : oof->fragment;
}

layout_point_t
layout_oof_positioned_offset(layout_oof_positioned_t* oof) {
  layout_point_t offset = {0.0, 0.0};

  return oof == NULL ? offset : oof->offset;
}

int layout_oof_positioned_stacking_order(layout_oof_positioned_t* oof) {
  return oof == NULL ? 0 : oof->stacking_order;
}

bool layout_oof_positioned_is_fixed(layout_oof_positioned_t* oof) {
  return oof != NULL && oof->fixed_position;
}

layout_fragment_placement_t
layout_oof_positioned_placement(layout_oof_positioned_t* oof) {
  return oof == NULL ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL : oof->placement;
}

layout_object_t*
layout_dom_node_create_layout_object(layout_t* layout, lxb_dom_node_t* node,
                                     const lxb_style_computed_t* style,
                                     unsigned internal_bits) {
  layout_object_t* object;

  if (layout == NULL || layout->objects == NULL || node == NULL || style == NULL) {
    return NULL;
  }

  object = layout_dom_node_layout_object(node);
  if (object != NULL) {
    if (object->layout != layout) {
      layout_dom_node_detach_layout_tree(node);
    } else {
      if (layout_object_set_style(object, style) != LXB_STATUS_OK) {
        return NULL;
      }
      layout_object_set_internal_type_bits(object, internal_bits);

      return object;
    }
  }

  object = lexbor_dobject_calloc(layout->objects);
  if (object == NULL) {
    return NULL;
  }

  object->layout = layout;
  object->node = node;
  object->id = layout_dom_object_id(layout, node);
  object->style = style;
  object->bitfields =
      LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS | internal_bits;
  layout_object_set_internal_type_bits(object, internal_bits);
  layout_object_init_fragment_data(object);
  lxb_style_computed_ref((lxb_style_computed_t*)object->style);
  layout_object_update_style_derived_bits(object);
  node->layout_object = object;

  return object;
}

layout_object_t*
layout_object_create_anonymous(layout_t* layout,
                               const lxb_style_computed_t* style) {
  layout_object_t* object;

  if (layout == NULL || layout->objects == NULL || style == NULL) {
    return NULL;
  }

  object = lexbor_dobject_calloc(layout->objects);
  if (object == NULL) {
    return NULL;
  }

  object->layout = layout;
  object->style = style;
  object->id = layout_anonymous_object_id(layout, object);
  object->bitfields = LAYOUT_OBJECT_ANONYMOUS | LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS;
  layout_object_init_fragment_data(object);
  lxb_style_computed_ref((lxb_style_computed_t*)object->style);
  layout_object_update_style_derived_bits(object);

  return object;
}

void layout_object_child_list_init(layout_object_child_list_t* children) {
  if (children == NULL) {
    return;
  }

  children->first_child = NULL;
  children->last_child = NULL;
}

lxb_status_t
layout_object_set_parent(layout_object_t* object, layout_object_t* parent) {
  if (object == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  object->parent = parent;
  return LXB_STATUS_OK;
}

lxb_status_t
layout_object_set_previous_sibling(layout_object_t* object,
                                   layout_object_t* previous) {
  if (object == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  object->prev = previous;
  return LXB_STATUS_OK;
}

lxb_status_t
layout_object_set_next_sibling(layout_object_t* object,
                               layout_object_t* next) {
  if (object == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  object->next = next;
  return LXB_STATUS_OK;
}

lxb_dom_node_t*
layout_object_node(layout_object_t* object) {
  if (object == NULL || (object->bitfields & LAYOUT_OBJECT_ANONYMOUS) != 0) {
    return NULL;
  }

  return object->node;
}

uint64_t
layout_object_id(layout_object_t* object) {
  return object == NULL ? 0 : object->id;
}

bool layout_object_is_anonymous(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_ANONYMOUS) != 0;
}

bool layout_object_can_have_children(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_INTERNAL_BLOCK) != 0;
}

lxb_status_t
layout_object_set_style(layout_object_t* object,
                        const lxb_style_computed_t* style) {
  if (object == NULL || style == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (object->style == style) {
    return LXB_STATUS_OK;
  }

  lxb_style_computed_ref((lxb_style_computed_t*)style);
  lxb_style_computed_unref((lxb_style_computed_t*)object->style);
  object->style = style;
  layout_object_update_style_derived_bits(object);
  layout_object_set_self_needs_full_layout(object, true);

  return LXB_STATUS_OK;
}

const lxb_style_computed_t*
layout_object_style(layout_object_t* object) {
  return object == NULL ? NULL : object->style;
}

static void
layout_object_set_bit(layout_object_t* object, unsigned bit, bool value) {
  if (object == NULL) {
    return;
  }

  if (value) {
    object->bitfields |= bit;
  } else {
    object->bitfields &= ~bit;
  }
}

void layout_object_set_pointer_events_none(layout_object_t* object, bool value) {
  layout_object_set_bit(object, LAYOUT_OBJECT_POINTER_EVENTS_NONE, value);
}

bool layout_object_pointer_events_none(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_POINTER_EVENTS_NONE) != 0;
}

void layout_object_set_has_clip_path(layout_object_t* object, bool value) {
  layout_object_set_bit(object, LAYOUT_OBJECT_HAS_CLIP_PATH, value);
}

bool layout_object_has_clip_path(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_HAS_CLIP_PATH) != 0;
}

void layout_object_set_can_contain_absolute_position(layout_object_t* object,
                                                     bool value) {
  layout_object_set_bit(object,
                        LAYOUT_OBJECT_CAN_CONTAIN_ABSOLUTE_POSITION,
                        value);
}

bool layout_object_can_contain_absolute_position(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_CAN_CONTAIN_ABSOLUTE_POSITION) != 0;
}

void layout_object_set_can_contain_fixed_position(layout_object_t* object,
                                                  bool value) {
  layout_object_set_bit(object, LAYOUT_OBJECT_CAN_CONTAIN_FIXED_POSITION, value);
}

bool layout_object_can_contain_fixed_position(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_CAN_CONTAIN_FIXED_POSITION) != 0;
}

void layout_object_set_column_spanner_container(layout_object_t* object,
                                                bool value) {
  layout_object_set_bit(object, LAYOUT_OBJECT_COLUMN_SPANNER_CONTAINER, value);
}

bool layout_object_is_column_spanner_container(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_COLUMN_SPANNER_CONTAINER) != 0;
}

void layout_object_set_column_spanner(layout_object_t* object, bool value) {
  layout_object_set_bit(object, LAYOUT_OBJECT_COLUMN_SPANNER, value);
}

bool layout_object_is_column_spanner(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_COLUMN_SPANNER) != 0;
}

void layout_object_set_can_traverse_physical_fragments(layout_object_t* object,
                                                       bool value) {
  layout_object_set_bit(object,
                        LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS,
                        value);
}

bool layout_object_can_traverse_physical_fragments(layout_object_t* object) {
  return object != NULL && (object->bitfields & LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS) != 0;
}

void layout_object_set_native_embed_token(layout_object_t* object,
                                          layout_native_embed_token_t token) {
  if (object == NULL) {
    return;
  }

  object->native_embed_token = token;
}

layout_native_embed_token_t
layout_object_native_embed_token(layout_object_t* object) {
  return object == NULL ? 0 : object->native_embed_token;
}

bool layout_object_is_native_embed_host(layout_object_t* object) {
  return layout_object_native_embed_token(object) != 0;
}

layout_object_t*
layout_object_parent(layout_object_t* object) {
  return object == NULL ? NULL : object->parent;
}

layout_object_t*
layout_object_previous_sibling(layout_object_t* object) {
  return object == NULL ? NULL : object->prev;
}

layout_object_t*
layout_object_next_sibling(layout_object_t* object) {
  return object == NULL ? NULL : object->next;
}

size_t
layout_object_depth(layout_object_t* object) {
  size_t depth = 0;

  for (layout_object_t* current = object; current != NULL;
       current = current->parent) {
    depth++;
  }

  return depth;
}

layout_object_t*
layout_object_common_ancestor(layout_object_t* object,
                              layout_object_t* other,
                              layout_object_common_ancestor_data_t* data) {
  size_t depth;
  size_t other_depth;
  layout_object_t* current = object;
  layout_object_t* other_current = other;
  layout_object_t* last = NULL;
  layout_object_t* other_last = NULL;

  if (data != NULL) {
    data->last = NULL;
    data->other_last = NULL;
  }

  if (object == NULL || other == NULL) {
    return NULL;
  }

  if (object == other) {
    return object;
  }

  depth = layout_object_depth(object);
  other_depth = layout_object_depth(other);

  if (depth > other_depth) {
    for (size_t i = depth - other_depth; i != 0; i--) {
      last = current;
      current = current->parent;
    }
  } else if (other_depth > depth) {
    for (size_t i = other_depth - depth; i != 0; i--) {
      other_last = other_current;
      other_current = other_current->parent;
    }
  }

  while (current != NULL && other_current != NULL) {
    if (current == other_current) {
      if (data != NULL) {
        data->last = last;
        data->other_last = other_last;
      }

      return current;
    }

    last = current;
    current = current->parent;
    other_last = other_current;
    other_current = other_current->parent;
  }

  return NULL;
}

bool layout_object_is_before_in_preorder(layout_object_t* object,
                                         layout_object_t* other) {
  layout_object_common_ancestor_data_t data;
  layout_object_t* common_ancestor;

  if (object == NULL || other == NULL || object == other) {
    return false;
  }

  common_ancestor = layout_object_common_ancestor(object, other, &data);
  if (common_ancestor == NULL) {
    return false;
  }

  if (data.last == NULL) {
    return true;
  }

  if (data.other_last == NULL) {
    return false;
  }

  for (layout_object_t* child = data.last; child != NULL;
       child = child->next) {
    if (child == data.other_last) {
      return true;
    }
  }

  return false;
}

layout_object_t*
layout_object_slow_first_child(layout_object_child_list_t* children) {
  return children == NULL ? NULL : children->first_child;
}

layout_object_t*
layout_object_slow_last_child(layout_object_child_list_t* children) {
  return children == NULL ? NULL : children->last_child;
}

lxb_status_t
layout_object_child_list_append(layout_object_child_list_t* children,
                                layout_object_t* parent,
                                layout_object_t* child) {
  return layout_object_child_list_insert_before(children, parent, child, NULL);
}

lxb_status_t
layout_object_child_list_insert_before(layout_object_child_list_t* children,
                                       layout_object_t* parent,
                                       layout_object_t* child,
                                       layout_object_t* before_child) {
  if (children == NULL || parent == NULL || child == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (child->parent != NULL || child->prev != NULL || child->next != NULL) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  while (before_child != NULL && before_child->parent != NULL && before_child->parent != parent) {
    before_child = before_child->parent;
  }

  if (before_child != NULL && before_child->parent != parent) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (before_child != NULL) {
    bool found = false;

    for (layout_object_t* current = children->first_child;
         current != NULL;
         current = current->next) {
      if (current == before_child) {
        found = true;
        break;
      }
    }

    if (!found) {
      return LXB_STATUS_ERROR_NOT_EXISTS;
    }
  }

  child->parent = parent;

  if (before_child != NULL) {
    child->prev = before_child->prev;
    child->next = before_child;

    if (before_child->prev != NULL) {
      before_child->prev->next = child;
    } else {
      children->first_child = child;
    }

    before_child->prev = child;
  } else {
    child->prev = children->last_child;
    child->next = NULL;

    if (children->last_child != NULL) {
      children->last_child->next = child;
    } else {
      children->first_child = child;
    }

    children->last_child = child;
  }

  if (children->last_child == NULL) {
    children->last_child = child;
  }

  layout_object_mark_child_needs_full_layout(parent);
  layout_object_mark_spanner_or_out_of_flow_parent_change(child);

  return LXB_STATUS_OK;
}

lxb_status_t
layout_object_child_list_remove(layout_object_child_list_t* children,
                                layout_object_t* child) {
  bool found = false;

  if (children == NULL || child == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  for (layout_object_t* current = children->first_child; current != NULL;
       current = current->next) {
    if (current == child) {
      found = true;
      break;
    }
  }

  if (!found) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  layout_object_mark_child_needs_full_layout(child->parent);
  layout_object_mark_spanner_or_out_of_flow_parent_change(child);

  if (child == children->first_child) {
    children->first_child = child->next;
  }

  if (child == children->last_child) {
    children->last_child = child->prev;
  }

  layout_object_unlink_from_siblings(child);
  child->parent = NULL;

  return LXB_STATUS_OK;
}

lxb_status_t
layout_object_child_list_clear(layout_object_child_list_t* children,
                               layout_object_t* parent) {
  lxb_status_t status;

  if (children == NULL || parent == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  while (children->first_child != NULL) {
    if (children->first_child->parent != parent) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    status = layout_object_child_list_remove(children,
                                             children->first_child);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  return LXB_STATUS_OK;
}

static int
layout_object_style_z_index(layout_object_t* object, bool* has_z_index) {
  const lxb_style_computed_non_inherited_t* non_inherited;

  if (has_z_index != NULL) {
    *has_z_index = false;
  }

  if (object == NULL || object->style == NULL || object->style->non_inherited == NULL) {
    return 0;
  }

  non_inherited = object->style->non_inherited;
  if (has_z_index != NULL) {
    *has_z_index = !non_inherited->z_index_auto;
  }

  return non_inherited->z_index;
}

lxb_status_t
layout_object_state(layout_object_t* object, layout_object_state_t* out_state) {
  const lxb_style_computed_non_inherited_t* non_inherited;

  if (object == NULL || out_state == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(out_state, 0, sizeof(layout_object_state_t));
  out_state->transform = layout_identity_transform;

  if (object->style == NULL) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  non_inherited = object->style->non_inherited;
  if (non_inherited != NULL) {
    out_state->display_box = non_inherited->display_box;
    out_state->display_outside = non_inherited->display_outside;
    out_state->display_inside = non_inherited->display_inside;
    out_state->position = non_inherited->position;
    out_state->overflow_x = non_inherited->overflow_x;
    out_state->overflow_y = non_inherited->overflow_y;
  }

  if (object->style->inherited != NULL) {
    out_state->visibility = object->style->inherited->visibility;
  }

  out_state->z_index = layout_object_style_z_index(object,
                                                   &out_state->has_z_index);

  return LXB_STATUS_OK;
}

layout_fragment_data_list_t*
layout_object_fragment_data_list(layout_object_t* object) {
  return object == NULL ? NULL : &object->fragment_data;
}

layout_fragment_data_t*
layout_object_first_fragment_data(layout_object_t* object) {
  return object == NULL ? NULL : &object->fragment_data.first;
}

size_t
layout_object_fragment_data_count(layout_object_t* object) {
  return object == NULL ? 0 : object->fragment_data.length;
}

bool layout_fragment_data_is_first(layout_fragment_data_t* fragment_data) {
  return fragment_data != NULL && (fragment_data->flags & LAYOUT_FRAGMENT_DATA_IS_FIRST) != 0;
}

void layout_fragment_data_set_paint_offset(layout_fragment_data_t* fragment_data,
                                           layout_point_t paint_offset) {
  if (fragment_data == NULL) {
    return;
  }

  fragment_data->paint_offset = paint_offset;
  fragment_data->flags |= LAYOUT_FRAGMENT_DATA_HAS_PAINT_OFFSET;
}

layout_point_t
layout_fragment_data_paint_offset(layout_fragment_data_t* fragment_data) {
  layout_point_t paint_offset = {0.0, 0.0};

  return fragment_data == NULL ? paint_offset : fragment_data->paint_offset;
}

layout_object_t*
layout_object_containing_block_for_absolute(layout_object_t* object) {
  if (object == NULL) {
    return NULL;
  }

  for (layout_object_t* ancestor = object->parent; ancestor != NULL;
       ancestor = layout_object_next_containing_block_candidate(ancestor)) {
    if (layout_object_establishes_absolute_cb(ancestor)) {
      return ancestor;
    }
  }

  return NULL;
}

layout_object_t*
layout_object_containing_block_for_fixed(layout_object_t* object) {
  if (object == NULL) {
    return NULL;
  }

  for (layout_object_t* ancestor = object->parent; ancestor != NULL;
       ancestor = layout_object_next_containing_block_candidate(ancestor)) {
    if (layout_object_establishes_fixed_cb(ancestor)) {
      return ancestor;
    }
  }

  return NULL;
}

layout_object_t*
layout_object_containing_block_for_column_spanner(layout_object_t* object) {
  if (object == NULL) {
    return NULL;
  }

  return layout_object_column_spanner_container(object);
}

bool layout_object_can_contain_out_of_flow_positioned(layout_object_t* object,
                                                      bool fixed_position) {
  if (object == NULL) {
    return false;
  }

  return fixed_position
             ? layout_object_can_contain_fixed_position(object)
             : layout_object_can_contain_absolute_position(object);
}

void layout_object_set_self_needs_full_layout(layout_object_t* object, bool value) {
  if (object == NULL) {
    return;
  }

  if (!value) {
    object->dirty_bits &= ~LAYOUT_DIRTY_SELF_FULL;
    return;
  }

  object->dirty_bits |= LAYOUT_DIRTY_SELF_FULL;
  for (layout_object_t* ancestor = object->parent; ancestor != NULL;
       ancestor = ancestor->parent) {
    ancestor->dirty_bits |= LAYOUT_DIRTY_CHILD_FULL;
  }
}

void layout_object_set_child_needs_full_layout(layout_object_t* object, bool value) {
  if (object == NULL) {
    return;
  }

  if (value) {
    object->dirty_bits |= LAYOUT_DIRTY_CHILD_FULL;
  } else {
    object->dirty_bits &= ~LAYOUT_DIRTY_CHILD_FULL;
  }
}

void layout_object_set_needs_simplified_layout(layout_object_t* object, bool value) {
  if (object == NULL) {
    return;
  }

  if (value) {
    object->dirty_bits |= LAYOUT_DIRTY_SIMPLIFIED;
  } else {
    object->dirty_bits &= ~LAYOUT_DIRTY_SIMPLIFIED;
  }
}

unsigned
layout_object_dirty_bits(layout_object_t* object) {
  return object == NULL ? LAYOUT_DIRTY_NONE : object->dirty_bits;
}

bool layout_object_self_needs_full_layout(layout_object_t* object) {
  return object != NULL && (object->dirty_bits & LAYOUT_DIRTY_SELF_FULL) != 0;
}

bool layout_object_child_needs_full_layout(layout_object_t* object) {
  return object != NULL && (object->dirty_bits & LAYOUT_DIRTY_CHILD_FULL) != 0;
}

bool layout_object_needs_simplified_layout(layout_object_t* object) {
  return object != NULL && (object->dirty_bits & LAYOUT_DIRTY_SIMPLIFIED) != 0;
}

bool layout_object_subtree_needs_layout(layout_object_t* object) {
  if (object == NULL) {
    return true;
  }

  return (object->dirty_bits & (LAYOUT_DIRTY_SELF_FULL | LAYOUT_DIRTY_CHILD_FULL | LAYOUT_DIRTY_SIMPLIFIED)) != 0;
}

void layout_object_clear_needs_layout(layout_object_t* object) {
  if (object == NULL) {
    return;
  }

  object->dirty_bits &= ~(LAYOUT_DIRTY_SELF_FULL | LAYOUT_DIRTY_CHILD_FULL | LAYOUT_DIRTY_SIMPLIFIED);
}

bool layout_result_needs_layout(layout_result_t* result) {
  if (result == NULL || result->root_fragment == NULL) {
    return true;
  }

  return layout_fragment_tree_needs_layout(result->root_fragment) || layout_result_out_of_flow_needs_layout(result);
}

static bool
layout_result_or_root_needs_layout(layout_result_t* result,
                                   layout_fragment_t* root_fragment) {
  if (result == NULL) {
    return true;
  }

  if (root_fragment != NULL) {
    return layout_fragment_tree_needs_layout(root_fragment) || layout_result_out_of_flow_needs_layout(result);
  }

  return layout_result_needs_layout(result);
}

layout_fragment_t*
layout_fragment_create(layout_result_t* result,
                       const layout_fragment_init_t* init) {
  layout_fragment_t* fragment;
  uint32_t fragment_index;

  if (result == NULL || result->fragments == NULL || init == NULL || init->object == NULL) {
    return NULL;
  }

  if (result->frozen) {
    return NULL;
  }

  if (!layout_result_next_fragment_index_for_object(result, init->object, &fragment_index)) {
    return NULL;
  }

  fragment = lexbor_dobject_calloc(result->fragments);
  if (fragment == NULL) {
    return NULL;
  }

  fragment->result = result;
  fragment->object = init->object;
  fragment->size = init->size;
  fragment->type = init->type;
  fragment->box_type = init->box_type;
  fragment->role = init->role == LAYOUT_FRAGMENT_ROLE_AUTO
                       ? layout_fragment_default_role(init->object, init->box_type)
                       : init->role;
  fragment->key.object_id = layout_object_id(init->object);
  fragment->key.fragment_index = fragment_index;
  fragment->key.role = (uint32_t)fragment->role;
  fragment->key.ordinal = fragment->key.fragment_index;
  fragment->flags = init->flags | LAYOUT_FRAGMENT_CHILDREN_VALID;
  fragment->stacking_order = init->stacking_order;
  fragment->transform = init->transform;
  fragment->margin = init->margin;
  fragment->border = init->border;
  fragment->padding = init->padding;
  fragment->border_sides = init->border_sides != LAYOUT_BOX_SIDE_NONE
                               ? init->border_sides
                               : layout_box_sides_from_edges(init->border);

  if (fragment->transform.a == 0.0 && fragment->transform.b == 0.0 && fragment->transform.c == 0.0 && fragment->transform.d == 0.0 && fragment->transform.e == 0.0 && fragment->transform.f == 0.0) {
    fragment->transform = layout_identity_transform;
  }

  return fragment;
}

static lxb_status_t
layout_fragment_clone_subtree_internal(layout_result_t* result,
                                       layout_fragment_t* source_fragment,
                                       layout_fragment_t** out_fragment) {
  layout_fragment_init_t init;
  layout_fragment_t* clone;

  if (out_fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  *out_fragment = NULL;

  /*
   * Blink keeps placement in PhysicalFragmentLink so a fragment subtree can
   * be reused independently of where a parent link places it.  This minimal
   * C version preserves that boundary by cloning the immutable subtree into
   * a new result; it does not share one fragment through multiple parents.
   */
  if (result == NULL || source_fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (result->frozen || source_fragment->result == NULL || !source_fragment->result->frozen || layout_fragment_tree_needs_layout(source_fragment)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (result->layout != source_fragment->result->layout) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  init.object = source_fragment->object;
  init.size = source_fragment->size;
  init.type = source_fragment->type;
  init.box_type = source_fragment->box_type;
  init.role = source_fragment->role;
  init.flags = source_fragment->flags & ~LAYOUT_FRAGMENT_CHILDREN_VALID;
  init.stacking_order = source_fragment->stacking_order;
  init.transform = source_fragment->transform;
  init.margin = source_fragment->margin;
  init.border = source_fragment->border;
  init.padding = source_fragment->padding;
  init.border_sides = source_fragment->border_sides;

  clone = layout_fragment_create(result, &init);
  if (clone == NULL) {
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  for (size_t i = 0; i < source_fragment->children_length; i++) {
    layout_fragment_link_t* source_link = &source_fragment->children[i];
    layout_fragment_t* child_clone;
    lxb_status_t status;

    if (source_link->fragment == NULL) {
      return LXB_STATUS_ERROR_WRONG_STAGE;
    }

    status = layout_fragment_clone_subtree_internal(result,
                                                    source_link->fragment,
                                                    &child_clone);
    if (status != LXB_STATUS_OK) {
      return status;
    }

    status = layout_fragment_append_child_with_placement(
        clone,
        child_clone,
        source_link->offset,
        source_link->placement,
        source_link->stacking_order);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  *out_fragment = clone;
  return LXB_STATUS_OK;
}

layout_fragment_t*
layout_fragment_clone_subtree(layout_result_t* result,
                              layout_fragment_t* source_fragment) {
  layout_fragment_t* clone = NULL;

  if (layout_fragment_clone_subtree_internal(result, source_fragment, &clone) != LXB_STATUS_OK) {
    return NULL;
  }

  return clone;
}

lxb_status_t
layout_fragment_append_cloned_child(layout_fragment_t* parent,
                                    const layout_fragment_link_t* source_link,
                                    layout_point_t offset,
                                    layout_fragment_t** out_cloned_child) {
  layout_fragment_t* clone;
  lxb_status_t status;

  if (out_cloned_child != NULL) {
    *out_cloned_child = NULL;
  }

  if (parent == NULL || parent->result == NULL || source_link == NULL || source_link->fragment == NULL) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  status = layout_fragment_clone_subtree_internal(parent->result,
                                                  source_link->fragment,
                                                  &clone);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  status = layout_fragment_append_child_with_placement(
      parent,
      clone,
      offset,
      source_link->placement,
      source_link->stacking_order);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (out_cloned_child != NULL) {
    *out_cloned_child = clone;
  }

  return LXB_STATUS_OK;
}

lxb_status_t
layout_fragment_append_child(layout_fragment_t* parent,
                             layout_fragment_t* child,
                             layout_point_t offset) {
  return layout_fragment_append_child_with_stacking_order(
      parent,
      child,
      offset,
      child == NULL ? 0 : child->stacking_order);
}

lxb_status_t
layout_fragment_append_child_with_stacking_order(layout_fragment_t* parent,
                                                 layout_fragment_t* child,
                                                 layout_point_t offset,
                                                 int stacking_order) {
  return layout_fragment_append_child_with_placement(
      parent,
      child,
      offset,
      LAYOUT_FRAGMENT_PLACEMENT_NORMAL,
      stacking_order);
}

lxb_status_t
layout_fragment_append_child_with_placement(
    layout_fragment_t* parent,
    layout_fragment_t* child,
    layout_point_t offset,
    layout_fragment_placement_t placement,
    int stacking_order) {
  layout_fragment_link_t* children;
  size_t capacity;

  if (parent == NULL || child == NULL || parent->result == NULL || parent->result->arena == NULL) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (parent->result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_fragment_children_valid(parent)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (child->result != parent->result || layout_fragment_tree_contains(parent, child) || layout_fragment_tree_contains(child, parent)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (parent->children_length == parent->children_capacity) {
    capacity = parent->children_capacity == 0
                   ? 4
                   : parent->children_capacity * 2;

    if (parent->children == NULL) {
      children = lexbor_mraw_calloc(parent->result->arena,
                                    capacity * sizeof(layout_fragment_link_t));
    } else {
      children = lexbor_mraw_realloc(parent->result->arena,
                                     parent->children,
                                     capacity * sizeof(layout_fragment_link_t));
    }

    if (children == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    parent->children = children;
    parent->children_capacity = capacity;
  }

  parent->children[parent->children_length].fragment = child;
  parent->children[parent->children_length].offset = offset;
  parent->children[parent->children_length].stacking_order = stacking_order;
  parent->children[parent->children_length].sequence =
      parent->children_length;
  parent->children[parent->children_length].placement = placement;
  parent->children_length++;

  return LXB_STATUS_OK;
}

int layout_fragment_stacking_order(layout_fragment_t* fragment) {
  return fragment == NULL ? 0 : fragment->stacking_order;
}

layout_fragment_link_t*
layout_fragment_first_child_link(layout_fragment_t* fragment) {
  if (!layout_fragment_children_valid(fragment) || fragment->children_length == 0) {
    return NULL;
  }

  return &fragment->children[0];
}

layout_fragment_link_t*
layout_fragment_last_child_link(layout_fragment_t* fragment) {
  if (!layout_fragment_children_valid(fragment) || fragment->children_length == 0) {
    return NULL;
  }

  return &fragment->children[fragment->children_length - 1];
}

size_t
layout_fragment_child_count(layout_fragment_t* fragment) {
  return layout_fragment_children_valid(fragment)
             ? fragment->children_length
             : 0;
}

layout_fragment_link_t*
layout_fragment_child_link_at(layout_fragment_t* fragment, size_t index) {
  if (!layout_fragment_children_valid(fragment) || index >= fragment->children_length) {
    return NULL;
  }

  return &fragment->children[index];
}

layout_fragment_t*
layout_fragment_link_fragment(layout_fragment_link_t* link) {
  return link == NULL ? NULL : link->fragment;
}

layout_point_t
layout_fragment_link_offset(layout_fragment_link_t* link) {
  return layout_fragment_link_offset_unsafe(link);
}

int layout_fragment_link_stacking_order(layout_fragment_link_t* link) {
  return layout_fragment_link_stacking_order_unsafe(link);
}

size_t
layout_fragment_link_sequence(layout_fragment_link_t* link) {
  return link == NULL ? 0 : link->sequence;
}

layout_fragment_placement_t
layout_fragment_link_placement(layout_fragment_link_t* link) {
  return link == NULL ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL : link->placement;
}

layout_object_t*
layout_fragment_object(layout_fragment_t* fragment) {
  return fragment == NULL ? NULL : fragment->object;
}

layout_fragment_key_t
layout_fragment_key(layout_fragment_t* fragment) {
  layout_fragment_key_t key = {0, 0, 0, 0};

  return fragment == NULL ? key : fragment->key;
}

layout_fragment_role_t
layout_fragment_role(layout_fragment_t* fragment) {
  return fragment == NULL ? LAYOUT_FRAGMENT_ROLE_NORMAL : fragment->role;
}

unsigned
layout_fragment_flags(layout_fragment_t* fragment) {
  return fragment == NULL ? 0 : fragment->flags;
}

bool layout_fragment_hidden_for_paint(layout_fragment_t* fragment) {
  return fragment != NULL && (fragment->flags & LAYOUT_FRAGMENT_HIDDEN_FOR_PAINT) != 0;
}

bool layout_fragment_opaque(layout_fragment_t* fragment) {
  return fragment != NULL && (fragment->flags & LAYOUT_FRAGMENT_OPAQUE) != 0;
}

bool layout_fragment_children_valid(layout_fragment_t* fragment) {
  return fragment != NULL && (fragment->flags & LAYOUT_FRAGMENT_CHILDREN_VALID) != 0;
}

layout_box_edges_t
layout_fragment_margin(layout_fragment_t* fragment) {
  layout_box_edges_t edges = {0.0, 0.0, 0.0, 0.0};

  return fragment == NULL ? edges : fragment->margin;
}

layout_box_edges_t
layout_fragment_border(layout_fragment_t* fragment) {
  layout_box_edges_t edges = {0.0, 0.0, 0.0, 0.0};

  return fragment == NULL ? edges : fragment->border;
}

layout_box_edges_t
layout_fragment_padding(layout_fragment_t* fragment) {
  layout_box_edges_t edges = {0.0, 0.0, 0.0, 0.0};

  return fragment == NULL ? edges : fragment->padding;
}

unsigned
layout_fragment_border_sides(layout_fragment_t* fragment) {
  return fragment == NULL ? LAYOUT_BOX_SIDE_NONE : fragment->border_sides;
}

layout_rect_t
layout_fragment_content_rect(layout_fragment_t* fragment) {
  return layout_fragment_content_rect_unsafe(fragment);
}

void layout_fragment_invalidate_children(layout_fragment_t* fragment) {
  if (fragment == NULL || !layout_fragment_children_valid(fragment)) {
    return;
  }

  for (size_t i = 0; i < fragment->children_length; i++) {
    fragment->children[i].fragment = NULL;
  }

  fragment->children = NULL;
  fragment->children_length = 0;
  fragment->children_capacity = 0;
  fragment->flags &= ~LAYOUT_FRAGMENT_CHILDREN_VALID;
}

lxb_status_t
layout_result_fragment_size(layout_result_t* result,
                            layout_fragment_t* fragment,
                            layout_size_t* out_size) {
  if (result == NULL || fragment == NULL || out_size == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (fragment->result != result) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (!result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (layout_result_or_root_needs_layout(result, result->root_fragment)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_object_can_traverse_physical_fragments(fragment->object)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  *out_size = layout_fragment_size_unsafe(fragment);
  return LXB_STATUS_OK;
}

lxb_status_t
layout_result_fragment_local_rect(layout_result_t* result,
                                  layout_fragment_t* fragment,
                                  layout_rect_t* out_rect) {
  if (result == NULL || fragment == NULL || out_rect == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (fragment->result != result) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (!result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (layout_result_or_root_needs_layout(result, result->root_fragment)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_object_can_traverse_physical_fragments(fragment->object)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  *out_rect = layout_fragment_local_rect_unsafe(fragment);
  return LXB_STATUS_OK;
}

lxb_status_t
layout_result_fragment_link_offset(layout_result_t* result,
                                   layout_fragment_t* parent_fragment,
                                   size_t index,
                                   layout_point_t* out_offset) {
  layout_fragment_link_t* link;

  if (result == NULL || parent_fragment == NULL || out_offset == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (parent_fragment->result != result) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (!result->frozen) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (layout_result_or_root_needs_layout(result, result->root_fragment)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_fragment_tree_can_traverse(parent_fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  link = layout_fragment_child_link_at(parent_fragment, index);
  if (link == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  *out_offset = layout_fragment_link_offset_unsafe(link);
  return LXB_STATUS_OK;
}

lxb_status_t
layout_result_fragment_for_object(layout_result_t* result,
                                  layout_object_t* target_object,
                                  size_t fragment_index,
                                  layout_fragment_t** out_fragment) {
  layout_fragment_t* fragment;
  lxb_status_t status;

  if (out_fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  *out_fragment = NULL;

  if (result == NULL || target_object == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_result_clean_fragment_read(result, result->root_fragment);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  fragment = layout_result_fragment_at_object(result->root_fragment,
                                              target_object,
                                              &fragment_index);
  if (fragment == NULL) {
    fragment = layout_result_oof_fragment_at_object(result, target_object, &fragment_index);
    if (fragment != NULL && !layout_fragment_tree_can_traverse(fragment)) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }
  } else if (!layout_fragment_path_can_traverse(result->root_fragment,
                                                fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (fragment == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  *out_fragment = fragment;
  return LXB_STATUS_OK;
}

static bool
layout_scene_fragment_key_equal(layout_fragment_key_t left,
                                layout_fragment_key_t right) {
  return left.object_id == right.object_id && left.fragment_index == right.fragment_index && left.role == right.role && left.ordinal == right.ordinal;
}

static layout_fragment_key_t
layout_scene_empty_key(void) {
  layout_fragment_key_t key = {0, 0, 0, 0};

  return key;
}

static bool
layout_scene_point_equal(layout_point_t left, layout_point_t right) {
  return left.x == right.x && left.y == right.y;
}

static bool
layout_scene_rect_equal(layout_rect_t left, layout_rect_t right) {
  return left.x == right.x && left.y == right.y && left.width == right.width && left.height == right.height;
}

static bool
layout_scene_transform_equal(layout_transform_t left,
                             layout_transform_t right) {
  return left.a == right.a && left.b == right.b && left.c == right.c && left.d == right.d && left.e == right.e && left.f == right.f;
}

static bool
layout_scene_transform_is_identity(layout_transform_t transform) {
  return layout_scene_transform_equal(transform, layout_identity_transform);
}

static double
layout_scene_object_opacity(layout_object_t* object) {
  if (object == NULL || object->style == NULL || object->style->non_inherited == NULL) {
    return 1.0;
  }

  return object->style->non_inherited->opacity;
}

static unsigned
layout_scene_dirty_from_object(layout_object_t* object) {
  if (object != NULL && layout_object_subtree_needs_layout(object)) {
    return LAYOUT_SCENE_DIRTY_LAYOUT;
  }

  return LAYOUT_SCENE_DIRTY_NONE;
}

static lxb_status_t
layout_scene_plan_reserve(layout_scene_plan_t* plan, size_t capacity) {
  layout_scene_node_t* nodes;

  if (plan == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (capacity <= plan->capacity) {
    return LXB_STATUS_OK;
  }

  if (plan->nodes == NULL) {
    nodes = lexbor_calloc(capacity, sizeof(layout_scene_node_t));
  } else {
    nodes = lexbor_realloc(plan->nodes,
                           capacity * sizeof(layout_scene_node_t));
  }

  if (nodes == NULL) {
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  plan->nodes = nodes;
  plan->capacity = capacity;

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_diff_reserve(layout_scene_diff_t* diff, size_t capacity) {
  layout_scene_patch_t* patches;

  if (diff == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (capacity <= diff->capacity) {
    return LXB_STATUS_OK;
  }

  if (diff->patches == NULL) {
    patches = lexbor_calloc(capacity, sizeof(layout_scene_patch_t));
  } else {
    patches = lexbor_realloc(diff->patches,
                             capacity * sizeof(layout_scene_patch_t));
  }

  if (patches == NULL) {
    return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  diff->patches = patches;
  diff->capacity = capacity;

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_diff_append(layout_scene_diff_t* diff,
                         layout_scene_patch_op_t op,
                         layout_fragment_key_t key,
                         layout_fragment_key_t old_parent_key,
                         layout_fragment_key_t new_parent_key,
                         layout_fragment_key_t old_previous_sibling_key,
                         layout_fragment_key_t new_previous_sibling_key,
                         size_t old_index,
                         size_t new_index,
                         size_t old_parent_index,
                         size_t new_parent_index,
                         size_t old_slot,
                         size_t new_slot,
                         size_t old_previous_sibling_index,
                         size_t new_previous_sibling_index,
                         size_t old_sequence,
                         size_t new_sequence,
                         layout_fragment_placement_t old_placement,
                         layout_fragment_placement_t new_placement,
                         int old_stacking_order,
                         int new_stacking_order,
                         unsigned dirty_bits) {
  layout_scene_patch_t* patch;
  size_t capacity;
  lxb_status_t status;

  if (diff == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (diff->length == diff->capacity) {
    capacity = diff->capacity == 0 ? LAYOUT_SCENE_PATCH_CHUNK
                                   : diff->capacity * 2;
    status = layout_scene_diff_reserve(diff, capacity);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  patch = &diff->patches[diff->length];
  memset(patch, 0, sizeof(*patch));
  patch->op = op;
  patch->key = key;
  patch->old_parent_key = old_parent_key;
  patch->new_parent_key = new_parent_key;
  patch->old_previous_sibling_key = old_previous_sibling_key;
  patch->new_previous_sibling_key = new_previous_sibling_key;
  patch->old_index = old_index;
  patch->new_index = new_index;
  patch->old_parent_index = old_parent_index;
  patch->new_parent_index = new_parent_index;
  patch->old_slot = old_slot;
  patch->new_slot = new_slot;
  patch->old_previous_sibling_index = old_previous_sibling_index;
  patch->new_previous_sibling_index = new_previous_sibling_index;
  patch->old_sequence = old_sequence;
  patch->new_sequence = new_sequence;
  patch->old_placement = old_placement;
  patch->new_placement = new_placement;
  patch->old_stacking_order = old_stacking_order;
  patch->new_stacking_order = new_stacking_order;
  patch->dirty_bits = dirty_bits;

  diff->length++;
  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_diff_append_node(layout_scene_diff_t* diff,
                              layout_scene_patch_op_t op,
                              const layout_scene_node_t* old_node,
                              const layout_scene_node_t* new_node,
                              unsigned dirty_bits) {
  layout_fragment_key_t key = layout_scene_empty_key();
  layout_fragment_key_t old_parent_key = layout_scene_empty_key();
  layout_fragment_key_t new_parent_key = layout_scene_empty_key();
  layout_fragment_key_t old_previous_sibling_key = layout_scene_empty_key();
  layout_fragment_key_t new_previous_sibling_key = layout_scene_empty_key();
  size_t old_index = LAYOUT_SCENE_NODE_NONE;
  size_t new_index = LAYOUT_SCENE_NODE_NONE;
  size_t old_parent_index = LAYOUT_SCENE_NODE_NONE;
  size_t new_parent_index = LAYOUT_SCENE_NODE_NONE;
  size_t old_slot = LAYOUT_SCENE_NODE_NONE;
  size_t new_slot = LAYOUT_SCENE_NODE_NONE;
  size_t old_previous_sibling_index = LAYOUT_SCENE_NODE_NONE;
  size_t new_previous_sibling_index = LAYOUT_SCENE_NODE_NONE;
  size_t old_sequence = 0;
  size_t new_sequence = 0;
  layout_fragment_placement_t old_placement =
      LAYOUT_FRAGMENT_PLACEMENT_NORMAL;
  layout_fragment_placement_t new_placement =
      LAYOUT_FRAGMENT_PLACEMENT_NORMAL;
  int old_stacking_order = 0;
  int new_stacking_order = 0;

  if (new_node != NULL) {
    key = new_node->key;
    new_parent_key = new_node->parent_key;
    new_previous_sibling_key = new_node->previous_sibling_key;
    new_index = new_node->index;
    new_parent_index = new_node->parent_index;
    new_slot = new_node->child_slot;
    new_previous_sibling_index = new_node->previous_sibling_index;
    new_sequence = new_node->sequence;
    new_placement = new_node->placement;
    new_stacking_order = new_node->stacking_order;
  } else if (old_node != NULL) {
    key = old_node->key;
  }

  if (old_node != NULL) {
    old_parent_key = old_node->parent_key;
    old_previous_sibling_key = old_node->previous_sibling_key;
    old_index = old_node->index;
    old_parent_index = old_node->parent_index;
    old_slot = old_node->child_slot;
    old_previous_sibling_index = old_node->previous_sibling_index;
    old_sequence = old_node->sequence;
    old_placement = old_node->placement;
    old_stacking_order = old_node->stacking_order;
  }

  return layout_scene_diff_append(
      diff,
      op,
      key,
      old_parent_key,
      new_parent_key,
      old_previous_sibling_key,
      new_previous_sibling_key,
      old_index,
      new_index,
      old_parent_index,
      new_parent_index,
      old_slot,
      new_slot,
      old_previous_sibling_index,
      new_previous_sibling_index,
      old_sequence,
      new_sequence,
      old_placement,
      new_placement,
      old_stacking_order,
      new_stacking_order,
      dirty_bits);
}

static void
layout_scene_node_fill(layout_scene_node_t* node,
                       layout_fragment_t* fragment,
                       size_t index,
                       size_t parent_index,
                       layout_point_t offset,
                       layout_fragment_placement_t placement,
                       size_t sequence,
                       int stacking_order) {
  layout_object_t* object;

  memset(node, 0, sizeof(*node));

  node->fragment = fragment;
  node->key = layout_fragment_key(fragment);
  node->index = index;
  node->parent_index = parent_index;
  node->first_child_index = LAYOUT_SCENE_NODE_NONE;
  node->last_child_index = LAYOUT_SCENE_NODE_NONE;
  node->previous_sibling_index = LAYOUT_SCENE_NODE_NONE;
  node->next_sibling_index = LAYOUT_SCENE_NODE_NONE;
  node->child_slot = LAYOUT_SCENE_NODE_NONE;
  node->offset = offset;
  node->local_rect = layout_fragment_local_rect_unsafe(fragment);
  node->transform = fragment == NULL ? layout_identity_transform
                                     : fragment->transform;
  node->clip_rect = node->local_rect;
  node->opacity = 1.0;
  node->placement = placement;
  node->sequence = sequence;
  node->stacking_order = stacking_order;

  object = layout_fragment_object(fragment);
  if (object == NULL) {
    return;
  }

  layout_object_hit_clip_axes(object, &node->clip_axes);
  node->opacity = layout_scene_object_opacity(object);
  node->dirty_bits = layout_scene_dirty_from_object(object);
  node->native_embed_token = layout_object_native_embed_token(object);

  if (layout_object_visible(object) && !layout_fragment_hidden_for_paint(fragment)) {
    node->flags |= LAYOUT_SCENE_NODE_VISIBLE;
  }
  if (layout_fragment_hidden_for_paint(fragment)) {
    node->flags |= LAYOUT_SCENE_NODE_HIDDEN_FOR_PAINT;
  }
  if (layout_object_pointer_events_none(object)) {
    node->flags |= LAYOUT_SCENE_NODE_POINTER_EVENTS_NONE;
  }
  if (layout_fragment_opaque(fragment)) {
    node->flags |= LAYOUT_SCENE_NODE_OPAQUE;
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_OPAQUE;
  }
  if (layout_object_has_clip_path(object)) {
    node->flags |= LAYOUT_SCENE_NODE_HAS_CLIP_PATH;
  }
  if (layout_object_is_native_embed_host(object)) {
    node->flags |= LAYOUT_SCENE_NODE_NATIVE_EMBED;
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_NATIVE_EMBED | LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY;
  }
  if (layout_fragment_establishes_stacking_context(fragment)) {
    node->flags |= LAYOUT_SCENE_NODE_STACKING_CONTEXT;
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY;
  }
  if (!layout_scene_transform_is_identity(node->transform)) {
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_TRANSFORM | LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY;
  }
  if (node->clip_axes != LAYOUT_OVERFLOW_CLIP_NONE) {
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_CLIP | LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY;
  }
  if (node->opacity < 1.0) {
    node->layer_hints |= LAYOUT_SCENE_LAYER_HINT_OPACITY | LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY;
  }
}

static lxb_status_t
layout_scene_plan_append_node(layout_scene_plan_t* plan,
                              layout_fragment_t* fragment,
                              size_t parent_index,
                              layout_point_t offset,
                              layout_fragment_placement_t placement,
                              size_t sequence,
                              int stacking_order,
                              size_t* out_index) {
  layout_scene_node_t* node;
  size_t index;
  size_t capacity;
  lxb_status_t status;

  if (out_index != NULL) {
    *out_index = LAYOUT_SCENE_NODE_NONE;
  }

  if (plan == NULL || fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (plan->length == plan->capacity) {
    capacity = plan->capacity == 0 ? LAYOUT_SCENE_NODE_CHUNK
                                   : plan->capacity * 2;
    status = layout_scene_plan_reserve(plan, capacity);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  index = plan->length;
  node = &plan->nodes[index];
  layout_scene_node_fill(node, fragment, index, parent_index, offset, placement, sequence, stacking_order);
  plan->length++;

  if (parent_index != LAYOUT_SCENE_NODE_NONE && parent_index < index) {
    layout_scene_node_t* parent = &plan->nodes[parent_index];

    node->parent_key = parent->key;
    node->child_slot = parent->child_count;

    if (parent->last_child_index != LAYOUT_SCENE_NODE_NONE) {
      layout_scene_node_t* previous =
          &plan->nodes[parent->last_child_index];
      previous->next_sibling_index = index;
      node->previous_sibling_index = previous->index;
      node->previous_sibling_key = previous->key;
    } else {
      parent->first_child_index = index;
    }

    parent->last_child_index = index;
    parent->child_count++;
  }

  if (out_index != NULL) {
    *out_index = index;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_plan_append_fragment_tree(layout_scene_plan_t* plan,
                                       layout_fragment_t* fragment,
                                       size_t parent_index,
                                       layout_point_t offset,
                                       layout_fragment_placement_t placement,
                                       size_t sequence,
                                       int stacking_order,
                                       size_t* out_index) {
  size_t index;
  size_t item_count;
  layout_visual_order_item_t* items = NULL;
  lxb_status_t status;

  status = layout_scene_plan_append_node(plan, fragment, parent_index, offset, placement, sequence, stacking_order, &index);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  item_count = layout_fragment_child_count(fragment) + layout_visual_order_out_of_flow_count(plan->result, fragment);
  if (item_count != 0) {
    items = lexbor_calloc(item_count,
                          sizeof(layout_visual_order_item_t));
    if (items == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    status = layout_visual_order_collect_items(
        plan->result,
        fragment,
        items,
        item_count,
        &item_count);
    if (status != LXB_STATUS_OK) {
      lexbor_free(items);
      return status;
    }
  }

  for (size_t visited_count = 0; visited_count < item_count;
       visited_count++) {
    layout_visual_order_item_t* item =
        layout_visual_order_next_item(items, item_count, false);

    if (item == NULL) {
      break;
    }

    if (item->fragment == NULL) {
      lexbor_free(items);
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    status = layout_scene_plan_append_fragment_tree(
        plan,
        item->fragment,
        index,
        item->offset,
        item->placement,
        item->sequence,
        item->stacking_order,
        NULL);
    if (status != LXB_STATUS_OK) {
      lexbor_free(items);
      return status;
    }
  }

  lexbor_free(items);

  if (out_index != NULL) {
    *out_index = index;
  }

  return LXB_STATUS_OK;
}

static size_t
layout_scene_plan_find_key_index(layout_scene_plan_t* plan,
                                 layout_fragment_key_t key) {
  if (plan == NULL) {
    return LAYOUT_SCENE_NODE_NONE;
  }

  for (size_t i = 0; i < plan->length; i++) {
    if (layout_scene_fragment_key_equal(plan->nodes[i].key, key)) {
      return i;
    }
  }

  return LAYOUT_SCENE_NODE_NONE;
}

static size_t
layout_scene_plan_child_at_slot(layout_scene_plan_t* plan,
                                const layout_scene_node_t* parent,
                                size_t slot) {
  size_t index;

  if (plan == NULL || parent == NULL || slot >= parent->child_count) {
    return LAYOUT_SCENE_NODE_NONE;
  }

  index = parent->first_child_index;
  for (size_t i = 0; i < slot; i++) {
    if (index == LAYOUT_SCENE_NODE_NONE || index >= plan->length) {
      return LAYOUT_SCENE_NODE_NONE;
    }

    index = plan->nodes[index].next_sibling_index;
  }

  return index;
}

static const layout_scene_node_t*
layout_scene_plan_parent_for_key(layout_scene_plan_t* plan,
                                 layout_fragment_key_t key) {
  size_t index = layout_scene_plan_find_key_index(plan, key);

  if (plan == NULL || index == LAYOUT_SCENE_NODE_NONE) {
    return NULL;
  }

  return &plan->nodes[index];
}

static bool
layout_scene_node_same_parent(layout_scene_plan_t* old_plan,
                              const layout_scene_node_t* old_node,
                              layout_scene_plan_t* new_plan,
                              const layout_scene_node_t* new_node) {
  if (old_node == NULL || new_node == NULL) {
    return false;
  }

  if (old_node->parent_index == LAYOUT_SCENE_NODE_NONE || new_node->parent_index == LAYOUT_SCENE_NODE_NONE) {
    return old_node->parent_index == LAYOUT_SCENE_NODE_NONE && new_node->parent_index == LAYOUT_SCENE_NODE_NONE;
  }

  if (old_plan == NULL || new_plan == NULL || old_node->parent_index >= old_plan->length || new_node->parent_index >= new_plan->length) {
    return false;
  }

  return layout_scene_fragment_key_equal(
      old_plan->nodes[old_node->parent_index].key,
      new_plan->nodes[new_node->parent_index].key);
}

static bool
layout_scene_node_same_slot(const layout_scene_node_t* old_node,
                            const layout_scene_node_t* new_node) {
  if (old_node == NULL || new_node == NULL) {
    return false;
  }

  return old_node->child_slot == new_node->child_slot && layout_scene_fragment_key_equal(old_node->previous_sibling_key, new_node->previous_sibling_key) && old_node->sequence == new_node->sequence && old_node->placement == new_node->placement && old_node->stacking_order == new_node->stacking_order;
}

static unsigned
layout_scene_node_diff_dirty(const layout_scene_node_t* old_node,
                             const layout_scene_node_t* new_node) {
  unsigned dirty = LAYOUT_SCENE_DIRTY_NONE;

  if (old_node == NULL || new_node == NULL) {
    return LAYOUT_SCENE_DIRTY_LAYOUT;
  }

  if (!layout_scene_point_equal(old_node->offset, new_node->offset) || !layout_scene_rect_equal(old_node->local_rect, new_node->local_rect) || old_node->stacking_order != new_node->stacking_order) {
    dirty |= LAYOUT_SCENE_DIRTY_GEOMETRY;
  }

  if (!layout_scene_transform_equal(old_node->transform,
                                    new_node->transform)) {
    dirty |= LAYOUT_SCENE_DIRTY_TRANSFORM | LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  if (old_node->clip_axes != new_node->clip_axes || !layout_scene_rect_equal(old_node->clip_rect,
                                                                             new_node->clip_rect)) {
    dirty |= LAYOUT_SCENE_DIRTY_CLIP | LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  if (old_node->opacity != new_node->opacity) {
    dirty |= LAYOUT_SCENE_DIRTY_OPACITY | LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  if (old_node->flags != new_node->flags) {
    dirty |= LAYOUT_SCENE_DIRTY_PAINT;

    if (((old_node->flags ^ new_node->flags) & LAYOUT_SCENE_NODE_NATIVE_EMBED) != 0) {
      dirty |= LAYOUT_SCENE_DIRTY_NATIVE_EMBED | LAYOUT_SCENE_DIRTY_COMPOSITING;
    }
  }

  if (old_node->layer_hints != new_node->layer_hints) {
    dirty |= LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  if (old_node->native_embed_token != new_node->native_embed_token) {
    dirty |= LAYOUT_SCENE_DIRTY_NATIVE_EMBED | LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  dirty |= old_node->dirty_bits | new_node->dirty_bits;

  return dirty;
}

layout_scene_plan_t*
layout_scene_plan_create(layout_t* layout) {
  layout_scene_plan_t* plan;

  if (layout == NULL) {
    return NULL;
  }

  plan = lexbor_calloc(1, sizeof(layout_scene_plan_t));
  if (plan == NULL) {
    return NULL;
  }

  if (layout_scene_plan_init(plan, layout) != LXB_STATUS_OK) {
    return layout_scene_plan_destroy(plan, true);
  }

  return plan;
}

lxb_status_t
layout_scene_plan_init(layout_scene_plan_t* plan, layout_t* layout) {
  if (plan == NULL || layout == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(plan, 0, sizeof(*plan));
  plan->layout = layout;

  return LXB_STATUS_OK;
}

layout_scene_plan_t*
layout_scene_plan_destroy(layout_scene_plan_t* plan, bool destroy_self) {
  if (plan == NULL) {
    return NULL;
  }

  lexbor_free(plan->nodes);

  if (destroy_self) {
    return lexbor_free(plan);
  }

  memset(plan, 0, sizeof(*plan));
  return plan;
}

void layout_scene_plan_clean(layout_scene_plan_t* plan) {
  layout_t* layout;

  if (plan == NULL) {
    return;
  }

  layout = plan->layout;
  lexbor_free(plan->nodes);
  memset(plan, 0, sizeof(*plan));
  plan->layout = layout;
}

layout_t*
layout_scene_plan_layout(layout_scene_plan_t* plan) {
  return plan == NULL ? NULL : plan->layout;
}

lxb_status_t
layout_scene_plan_build(layout_scene_plan_t* plan, layout_result_t* result) {
  layout_point_t root_offset = {0.0, 0.0};
  lxb_status_t status;

  if (plan == NULL || result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (plan->layout != result->layout) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (result->root_fragment == NULL || !result->frozen || layout_result_needs_layout(result)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_fragment_tree_can_traverse(result->root_fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (!layout_fragment_tree_can_traverse(result->oofs[i].fragment)) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }
    if (layout_result_oof_containing_fragment(result, &result->oofs[i]) == NULL) {
      return LXB_STATUS_ERROR_NOT_EXISTS;
    }
  }

  layout_scene_plan_clean(plan);
  plan->result = result;
  plan->generation = layout_result_generation(result);

  status = layout_scene_plan_append_fragment_tree(
      plan,
      result->root_fragment,
      LAYOUT_SCENE_NODE_NONE,
      root_offset,
      LAYOUT_FRAGMENT_PLACEMENT_NORMAL,
      0,
      layout_fragment_stacking_order(result->root_fragment),
      NULL);
  if (status != LXB_STATUS_OK) {
    layout_scene_plan_clean(plan);
    return status;
  }

  return LXB_STATUS_OK;
}

uint64_t
layout_scene_plan_generation(layout_scene_plan_t* plan) {
  return plan == NULL ? 0 : plan->generation;
}

size_t
layout_scene_plan_node_count(layout_scene_plan_t* plan) {
  return plan == NULL ? 0 : plan->length;
}

const layout_scene_node_t*
layout_scene_plan_node_at(layout_scene_plan_t* plan, size_t index) {
  if (plan == NULL || index >= plan->length) {
    return NULL;
  }

  return &plan->nodes[index];
}

size_t
layout_scene_node_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->index;
}

size_t
layout_scene_node_parent_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->parent_index;
}

size_t
layout_scene_node_first_child_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->first_child_index;
}

size_t
layout_scene_node_last_child_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->last_child_index;
}

size_t
layout_scene_node_previous_sibling_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE
                      : node->previous_sibling_index;
}

size_t
layout_scene_node_next_sibling_index(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->next_sibling_index;
}

size_t
layout_scene_node_child_count(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->child_count;
}

layout_fragment_key_t
layout_scene_node_key(const layout_scene_node_t* node) {
  return node == NULL ? layout_scene_empty_key() : node->key;
}

layout_fragment_key_t
layout_scene_node_parent_key(const layout_scene_node_t* node) {
  return node == NULL ? layout_scene_empty_key() : node->parent_key;
}

size_t
layout_scene_node_child_slot(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_NODE_NONE : node->child_slot;
}

layout_fragment_key_t
layout_scene_node_previous_sibling_key(const layout_scene_node_t* node) {
  return node == NULL ? layout_scene_empty_key()
                      : node->previous_sibling_key;
}

layout_point_t
layout_scene_node_offset(const layout_scene_node_t* node) {
  layout_point_t offset = {0.0, 0.0};

  return node == NULL ? offset : node->offset;
}

layout_rect_t
layout_scene_node_local_rect(const layout_scene_node_t* node) {
  layout_rect_t rect = {0.0, 0.0, 0.0, 0.0};

  return node == NULL ? rect : node->local_rect;
}

layout_transform_t
layout_scene_node_transform(const layout_scene_node_t* node) {
  return node == NULL ? layout_identity_transform : node->transform;
}

layout_overflow_clip_t
layout_scene_node_clip_axes(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_OVERFLOW_CLIP_NONE : node->clip_axes;
}

layout_rect_t
layout_scene_node_clip_rect(const layout_scene_node_t* node) {
  layout_rect_t rect = {0.0, 0.0, 0.0, 0.0};

  return node == NULL ? rect : node->clip_rect;
}

double
layout_scene_node_opacity(const layout_scene_node_t* node) {
  return node == NULL ? 1.0 : node->opacity;
}

unsigned
layout_scene_node_flags(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->flags;
}

unsigned
layout_scene_node_layer_hints(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->layer_hints;
}

unsigned
layout_scene_node_dirty_bits(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_SCENE_DIRTY_NONE : node->dirty_bits;
}

layout_fragment_placement_t
layout_scene_node_placement(const layout_scene_node_t* node) {
  return node == NULL ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL : node->placement;
}

size_t
layout_scene_node_sequence(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->sequence;
}

int layout_scene_node_stacking_order(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->stacking_order;
}

layout_native_embed_token_t
layout_scene_node_native_embed_token(const layout_scene_node_t* node) {
  return node == NULL ? 0 : node->native_embed_token;
}

layout_scene_diff_t*
layout_scene_diff_create(void) {
  layout_scene_diff_t* diff = lexbor_calloc(1, sizeof(layout_scene_diff_t));

  if (diff == NULL) {
    return NULL;
  }

  if (layout_scene_diff_init(diff) != LXB_STATUS_OK) {
    return layout_scene_diff_destroy(diff, true);
  }

  return diff;
}

lxb_status_t
layout_scene_diff_init(layout_scene_diff_t* diff) {
  if (diff == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  memset(diff, 0, sizeof(*diff));
  return LXB_STATUS_OK;
}

layout_scene_diff_t*
layout_scene_diff_destroy(layout_scene_diff_t* diff, bool destroy_self) {
  if (diff == NULL) {
    return NULL;
  }

  lexbor_free(diff->patches);

  if (destroy_self) {
    return lexbor_free(diff);
  }

  memset(diff, 0, sizeof(*diff));
  return diff;
}

void layout_scene_diff_clean(layout_scene_diff_t* diff) {
  if (diff == NULL) {
    return;
  }

  lexbor_free(diff->patches);
  memset(diff, 0, sizeof(*diff));
}

static size_t
layout_scene_plan_find_unused_key_index(layout_scene_plan_t* plan,
                                        layout_fragment_key_t key,
                                        const bool* used) {
  if (plan == NULL) {
    return LAYOUT_SCENE_NODE_NONE;
  }

  for (size_t i = 0; i < plan->length; i++) {
    if ((used == NULL || !used[i]) && layout_scene_fragment_key_equal(plan->nodes[i].key, key)) {
      return i;
    }
  }

  return LAYOUT_SCENE_NODE_NONE;
}

static size_t
layout_scene_plan_find_unused_child_key_index(layout_scene_plan_t* plan,
                                              const layout_scene_node_t* parent,
                                              size_t first_slot,
                                              size_t last_slot,
                                              layout_fragment_key_t key,
                                              const bool* used) {
  if (plan == NULL || parent == NULL || first_slot > last_slot || last_slot > parent->child_count) {
    return LAYOUT_SCENE_NODE_NONE;
  }

  for (size_t slot = first_slot; slot < last_slot; slot++) {
    size_t index = layout_scene_plan_child_at_slot(plan, parent, slot);

    if (index == LAYOUT_SCENE_NODE_NONE || index >= plan->length) {
      return LAYOUT_SCENE_NODE_NONE;
    }

    if ((used == NULL || !used[index]) && layout_scene_fragment_key_equal(plan->nodes[index].key, key)) {
      return index;
    }
  }

  return LAYOUT_SCENE_NODE_NONE;
}

static lxb_status_t
layout_scene_diff_emit_insert(layout_scene_diff_t* diff,
                              const layout_scene_node_t* new_node,
                              bool* new_used) {
  lxb_status_t status;
  unsigned dirty = LAYOUT_SCENE_DIRTY_CHILD_LIST | LAYOUT_SCENE_DIRTY_GEOMETRY | LAYOUT_SCENE_DIRTY_PAINT;

  if (new_node == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  dirty |= new_node->dirty_bits;

  if (new_node->layer_hints != LAYOUT_SCENE_LAYER_HINT_NONE) {
    dirty |= LAYOUT_SCENE_DIRTY_COMPOSITING;
  }

  status = layout_scene_diff_append_node(diff, LAYOUT_SCENE_PATCH_INSERT, NULL, new_node, dirty);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (new_used != NULL) {
    new_used[new_node->index] = true;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_diff_emit_remove(layout_scene_diff_t* diff,
                              const layout_scene_node_t* old_node,
                              bool* old_used) {
  lxb_status_t status;

  if (old_node == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_scene_diff_append_node(
      diff,
      LAYOUT_SCENE_PATCH_REMOVE,
      old_node,
      NULL,
      LAYOUT_SCENE_DIRTY_CHILD_LIST);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (old_used != NULL) {
    old_used[old_node->index] = true;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_diff_emit_update(layout_scene_plan_t* old_plan,
                              size_t old_index,
                              layout_scene_plan_t* new_plan,
                              size_t new_index,
                              layout_scene_diff_t* diff,
                              bool* old_used,
                              bool* new_used) {
  const layout_scene_node_t* old_node;
  const layout_scene_node_t* new_node;
  layout_scene_patch_op_t op;
  unsigned dirty;
  bool moved;
  lxb_status_t status;

  if (old_plan == NULL || new_plan == NULL || diff == NULL || old_index >= old_plan->length || new_index >= new_plan->length) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  old_node = &old_plan->nodes[old_index];
  new_node = &new_plan->nodes[new_index];
  dirty = layout_scene_node_diff_dirty(old_node, new_node);
  moved = !layout_scene_node_same_parent(old_plan, old_node, new_plan, new_node) || !layout_scene_node_same_slot(old_node, new_node);

  if (moved) {
    op = LAYOUT_SCENE_PATCH_MOVE;
    dirty |= LAYOUT_SCENE_DIRTY_CHILD_LIST;
  } else if (dirty != LAYOUT_SCENE_DIRTY_NONE) {
    op = LAYOUT_SCENE_PATCH_UPDATE;
  } else {
    op = LAYOUT_SCENE_PATCH_KEEP;
  }

  status = layout_scene_diff_append_node(diff, op, old_node, new_node, dirty);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (old_used != NULL) {
    old_used[old_index] = true;
  }
  if (new_used != NULL) {
    new_used[new_index] = true;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_scene_plan_diff_children(layout_scene_plan_t* old_plan,
                                const layout_scene_node_t* old_parent,
                                layout_scene_plan_t* new_plan,
                                const layout_scene_node_t* new_parent,
                                layout_scene_diff_t* diff,
                                bool* old_used,
                                bool* new_used) {
  size_t old_top = 0;
  size_t new_top = 0;
  size_t old_bottom;
  size_t new_bottom;
  lxb_status_t status;

  if (old_plan == NULL || new_plan == NULL || old_parent == NULL || new_parent == NULL || diff == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  old_bottom = old_parent->child_count;
  new_bottom = new_parent->child_count;

  while (old_top < old_bottom && new_top < new_bottom) {
    size_t old_index =
        layout_scene_plan_child_at_slot(old_plan, old_parent, old_top);
    size_t new_index =
        layout_scene_plan_child_at_slot(new_plan, new_parent, new_top);

    if (old_index == LAYOUT_SCENE_NODE_NONE || new_index == LAYOUT_SCENE_NODE_NONE) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    if (!layout_scene_fragment_key_equal(old_plan->nodes[old_index].key,
                                         new_plan->nodes[new_index].key)) {
      break;
    }

    status = layout_scene_diff_emit_update(
        old_plan,
        old_index,
        new_plan,
        new_index,
        diff,
        old_used,
        new_used);
    if (status != LXB_STATUS_OK) {
      return status;
    }

    old_top++;
    new_top++;
  }

  while (old_top < old_bottom && new_top < new_bottom) {
    size_t old_index = layout_scene_plan_child_at_slot(
        old_plan,
        old_parent,
        old_bottom - 1);
    size_t new_index = layout_scene_plan_child_at_slot(
        new_plan,
        new_parent,
        new_bottom - 1);

    if (old_index == LAYOUT_SCENE_NODE_NONE || new_index == LAYOUT_SCENE_NODE_NONE) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    if (!layout_scene_fragment_key_equal(old_plan->nodes[old_index].key,
                                         new_plan->nodes[new_index].key)) {
      break;
    }

    old_bottom--;
    new_bottom--;
  }

  for (size_t slot = new_top; slot < new_bottom; slot++) {
    size_t new_index =
        layout_scene_plan_child_at_slot(new_plan, new_parent, slot);
    size_t old_index;

    if (new_index == LAYOUT_SCENE_NODE_NONE) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    old_index = layout_scene_plan_find_unused_child_key_index(
        old_plan,
        old_parent,
        old_top,
        old_bottom,
        new_plan->nodes[new_index].key,
        old_used);
    if (old_index == LAYOUT_SCENE_NODE_NONE) {
      old_index = layout_scene_plan_find_unused_key_index(
          old_plan,
          new_plan->nodes[new_index].key,
          old_used);
    }

    if (old_index != LAYOUT_SCENE_NODE_NONE) {
      status = layout_scene_diff_emit_update(
          old_plan,
          old_index,
          new_plan,
          new_index,
          diff,
          old_used,
          new_used);
    } else {
      status = layout_scene_diff_emit_insert(diff,
                                             &new_plan->nodes[new_index],
                                             new_used);
    }

    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  for (size_t slot = new_bottom; slot < new_parent->child_count; slot++) {
    size_t old_slot = old_bottom + (slot - new_bottom);
    size_t old_index =
        layout_scene_plan_child_at_slot(old_plan, old_parent, old_slot);
    size_t new_index =
        layout_scene_plan_child_at_slot(new_plan, new_parent, slot);

    if (old_index == LAYOUT_SCENE_NODE_NONE || new_index == LAYOUT_SCENE_NODE_NONE) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    status = layout_scene_diff_emit_update(
        old_plan,
        old_index,
        new_plan,
        new_index,
        diff,
        old_used,
        new_used);
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  return LXB_STATUS_OK;
}

lxb_status_t
layout_scene_plan_diff(layout_scene_plan_t* old_plan,
                       layout_scene_plan_t* new_plan,
                       layout_scene_diff_t* diff) {
  lxb_status_t status;
  bool* old_used = NULL;
  bool* new_used = NULL;

  if (diff == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (old_plan == NULL && new_plan == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (old_plan != NULL && new_plan != NULL && old_plan->layout != new_plan->layout) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  layout_scene_diff_clean(diff);
  diff->old_generation = layout_scene_plan_generation(old_plan);
  diff->new_generation = layout_scene_plan_generation(new_plan);

  if (old_plan != NULL && new_plan == NULL) {
    for (size_t i = 0; i < old_plan->length; i++) {
      status = layout_scene_diff_emit_remove(diff, &old_plan->nodes[i], NULL);
      if (status != LXB_STATUS_OK) {
        return status;
      }
    }
    return LXB_STATUS_OK;
  }

  if (old_plan == NULL) {
    for (size_t i = 0; i < new_plan->length; i++) {
      status = layout_scene_diff_emit_insert(diff, &new_plan->nodes[i], NULL);
      if (status != LXB_STATUS_OK) {
        return status;
      }
    }
    return LXB_STATUS_OK;
  }

  if (old_plan->length != 0) {
    old_used = lexbor_calloc(old_plan->length, sizeof(bool));
    if (old_used == NULL) {
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }
  }
  if (new_plan->length != 0) {
    new_used = lexbor_calloc(new_plan->length, sizeof(bool));
    if (new_used == NULL) {
      lexbor_free(old_used);
      return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }
  }

  for (size_t i = 0; i < new_plan->length; i++) {
    const layout_scene_node_t* new_node = &new_plan->nodes[i];
    size_t old_index;

    if (new_node->parent_index != LAYOUT_SCENE_NODE_NONE) {
      continue;
    }

    old_index = layout_scene_plan_find_unused_key_index(
        old_plan,
        new_node->key,
        old_used);
    if (old_index != LAYOUT_SCENE_NODE_NONE && old_plan->nodes[old_index].parent_index == LAYOUT_SCENE_NODE_NONE) {
      status = layout_scene_diff_emit_update(
          old_plan,
          old_index,
          new_plan,
          i,
          diff,
          old_used,
          new_used);
    } else {
      status = layout_scene_diff_emit_insert(diff, new_node, new_used);
    }

    if (status != LXB_STATUS_OK) {
      lexbor_free(new_used);
      lexbor_free(old_used);
      return status;
    }
  }

  for (size_t i = 0; i < new_plan->length; i++) {
    const layout_scene_node_t* new_parent = &new_plan->nodes[i];
    const layout_scene_node_t* old_parent =
        layout_scene_plan_parent_for_key(old_plan, new_parent->key);

    if (new_parent->child_count == 0 || old_parent == NULL) {
      continue;
    }

    status = layout_scene_plan_diff_children(
        old_plan,
        old_parent,
        new_plan,
        new_parent,
        diff,
        old_used,
        new_used);
    if (status != LXB_STATUS_OK) {
      lexbor_free(new_used);
      lexbor_free(old_used);
      return status;
    }
  }

  for (size_t i = 0; i < new_plan->length; i++) {
    size_t old_index;

    if (new_used[i]) {
      continue;
    }

    old_index = layout_scene_plan_find_unused_key_index(
        old_plan,
        new_plan->nodes[i].key,
        old_used);
    if (old_index != LAYOUT_SCENE_NODE_NONE) {
      status = layout_scene_diff_emit_update(
          old_plan,
          old_index,
          new_plan,
          i,
          diff,
          old_used,
          new_used);
    } else {
      status = layout_scene_diff_emit_insert(diff, &new_plan->nodes[i], new_used);
    }

    if (status != LXB_STATUS_OK) {
      lexbor_free(new_used);
      lexbor_free(old_used);
      return status;
    }
  }

  for (size_t i = 0; i < old_plan->length; i++) {
    if (!old_used[i]) {
      status = layout_scene_diff_emit_remove(diff, &old_plan->nodes[i], old_used);
      if (status != LXB_STATUS_OK) {
        lexbor_free(new_used);
        lexbor_free(old_used);
        return status;
      }
    }
  }

  lexbor_free(new_used);
  lexbor_free(old_used);
  return LXB_STATUS_OK;
}

uint64_t
layout_scene_diff_old_generation(layout_scene_diff_t* diff) {
  return diff == NULL ? 0 : diff->old_generation;
}

uint64_t
layout_scene_diff_new_generation(layout_scene_diff_t* diff) {
  return diff == NULL ? 0 : diff->new_generation;
}

size_t
layout_scene_diff_patch_count(layout_scene_diff_t* diff) {
  return diff == NULL ? 0 : diff->length;
}

const layout_scene_patch_t*
layout_scene_diff_patch_at(layout_scene_diff_t* diff, size_t index) {
  if (diff == NULL || index >= diff->length) {
    return NULL;
  }

  return &diff->patches[index];
}

layout_scene_patch_op_t
layout_scene_patch_op(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_PATCH_KEEP : patch->op;
}

layout_fragment_key_t
layout_scene_patch_key(const layout_scene_patch_t* patch) {
  return patch == NULL ? layout_scene_empty_key() : patch->key;
}

size_t
layout_scene_patch_old_index(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->old_index;
}

size_t
layout_scene_patch_new_index(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->new_index;
}

size_t
layout_scene_patch_old_parent_index(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->old_parent_index;
}

size_t
layout_scene_patch_new_parent_index(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->new_parent_index;
}

layout_fragment_key_t
layout_scene_patch_old_parent_key(const layout_scene_patch_t* patch) {
  return patch == NULL ? layout_scene_empty_key() : patch->old_parent_key;
}

layout_fragment_key_t
layout_scene_patch_new_parent_key(const layout_scene_patch_t* patch) {
  return patch == NULL ? layout_scene_empty_key() : patch->new_parent_key;
}

size_t
layout_scene_patch_old_slot(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->old_slot;
}

size_t
layout_scene_patch_new_slot(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE : patch->new_slot;
}

size_t
layout_scene_patch_old_previous_sibling_index(
    const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE
                       : patch->old_previous_sibling_index;
}

size_t
layout_scene_patch_new_previous_sibling_index(
    const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_NODE_NONE
                       : patch->new_previous_sibling_index;
}

layout_fragment_key_t
layout_scene_patch_old_previous_sibling_key(
    const layout_scene_patch_t* patch) {
  return patch == NULL ? layout_scene_empty_key()
                       : patch->old_previous_sibling_key;
}

layout_fragment_key_t
layout_scene_patch_new_previous_sibling_key(
    const layout_scene_patch_t* patch) {
  return patch == NULL ? layout_scene_empty_key()
                       : patch->new_previous_sibling_key;
}

size_t
layout_scene_patch_old_sequence(const layout_scene_patch_t* patch) {
  return patch == NULL ? 0 : patch->old_sequence;
}

size_t
layout_scene_patch_new_sequence(const layout_scene_patch_t* patch) {
  return patch == NULL ? 0 : patch->new_sequence;
}

layout_fragment_placement_t
layout_scene_patch_old_placement(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL
                       : patch->old_placement;
}

layout_fragment_placement_t
layout_scene_patch_new_placement(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL
                       : patch->new_placement;
}

int layout_scene_patch_old_stacking_order(const layout_scene_patch_t* patch) {
  return patch == NULL ? 0 : patch->old_stacking_order;
}

int layout_scene_patch_new_stacking_order(const layout_scene_patch_t* patch) {
  return patch == NULL ? 0 : patch->new_stacking_order;
}

unsigned
layout_scene_patch_dirty_bits(const layout_scene_patch_t* patch) {
  return patch == NULL ? LAYOUT_SCENE_DIRTY_NONE : patch->dirty_bits;
}

layout_overflow_clip_t
layout_object_overflow_clip(layout_object_t* object) {
  layout_overflow_clip_t clip = LAYOUT_OVERFLOW_CLIP_NONE;
  const lxb_style_computed_non_inherited_t* non_inherited;

  if (object == NULL || object->style == NULL || object->style->non_inherited == NULL) {
    return clip;
  }

  non_inherited = object->style->non_inherited;

  if (non_inherited->overflow_x == LXB_CSS_OVERFLOW_X_HIDDEN || non_inherited->overflow_x == LXB_CSS_OVERFLOW_X_CLIP || non_inherited->overflow_x == LXB_CSS_OVERFLOW_X_SCROLL || non_inherited->overflow_x == LXB_CSS_OVERFLOW_X_AUTO) {
    clip = (layout_overflow_clip_t)(clip | LAYOUT_OVERFLOW_CLIP_X);
  }

  if (non_inherited->overflow_y == LXB_CSS_OVERFLOW_Y_HIDDEN || non_inherited->overflow_y == LXB_CSS_OVERFLOW_Y_CLIP || non_inherited->overflow_y == LXB_CSS_OVERFLOW_Y_SCROLL || non_inherited->overflow_y == LXB_CSS_OVERFLOW_Y_AUTO) {
    clip = (layout_overflow_clip_t)(clip | LAYOUT_OVERFLOW_CLIP_Y);
  }

  return clip;
}

bool layout_transform_invert(const layout_transform_t* transform,
                             layout_transform_t* out) {
  double det;

  if (transform == NULL || out == NULL) {
    return false;
  }

  det = (transform->a * transform->d) - (transform->b * transform->c);
  if (fabs(det) < 0.0000001) {
    return false;
  }

  out->a = transform->d / det;
  out->b = -transform->b / det;
  out->c = -transform->c / det;
  out->d = transform->a / det;
  out->e = ((transform->c * transform->f) - (transform->d * transform->e)) / det;
  out->f = ((transform->b * transform->e) - (transform->a * transform->f)) / det;

  return true;
}

layout_point_t
layout_transform_map_point(const layout_transform_t* transform,
                           layout_point_t point) {
  layout_point_t mapped = point;

  if (transform == NULL) {
    return mapped;
  }

  mapped.x = (transform->a * point.x) + (transform->c * point.y) + transform->e;
  mapped.y = (transform->b * point.x) + (transform->d * point.y) + transform->f;

  return mapped;
}

static layout_rect_t
layout_rect_from_points(const layout_point_t* points, size_t length) {
  layout_rect_t rect = {0.0, 0.0, 0.0, 0.0};
  double min_x;
  double min_y;
  double max_x;
  double max_y;

  if (points == NULL || length == 0) {
    return rect;
  }

  min_x = points[0].x;
  min_y = points[0].y;
  max_x = points[0].x;
  max_y = points[0].y;

  for (size_t i = 1; i < length; i++) {
    if (points[i].x < min_x) {
      min_x = points[i].x;
    }
    if (points[i].x > max_x) {
      max_x = points[i].x;
    }
    if (points[i].y < min_y) {
      min_y = points[i].y;
    }
    if (points[i].y > max_y) {
      max_y = points[i].y;
    }
  }

  rect.x = min_x;
  rect.y = min_y;
  rect.width = max_x - min_x;
  rect.height = max_y - min_y;

  return rect;
}

static lxb_status_t
layout_fragment_point_from_root(layout_fragment_t* root_fragment,
                                layout_fragment_t* target_fragment,
                                layout_point_t root_point,
                                unsigned coordinate_flags,
                                layout_point_t* out_local) {
  const layout_fragment_link_t* links[128];
  size_t length = 0;
  layout_point_t point = root_point;

  if (root_fragment == NULL || target_fragment == NULL || out_local == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (!layout_fragment_find_path(root_fragment, target_fragment, links, &length, sizeof(links) / sizeof(links[0]))) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  if (!layout_fragment_ancestor_to_local(root_fragment, point, coordinate_flags, &point)) {
    return LXB_STATUS_ERROR_UNEXPECTED_DATA;
  }

  for (size_t i = 0; i < length; i++) {
    const layout_fragment_link_t* link = links[i];

    point.x -= link->offset.x;
    point.y -= link->offset.y;

    if (!layout_fragment_ancestor_to_local(link->fragment, point, coordinate_flags, &point)) {
      return LXB_STATUS_ERROR_UNEXPECTED_DATA;
    }
  }

  *out_local = point;

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_result_clean_fragment_read(layout_result_t* result,
                                  layout_fragment_t* fragment) {
  if (result == NULL || fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (result->root_fragment == NULL || !result->frozen || layout_result_or_root_needs_layout(result, result->root_fragment)) {
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (fragment->result != result || result->root_fragment->result != result) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  return LXB_STATUS_OK;
}

static lxb_status_t
layout_result_root_point_to_oof_fragment(layout_result_t* result,
                                         layout_oof_positioned_t* oof,
                                         layout_point_t root_point,
                                         unsigned coordinate_flags,
                                         layout_point_t* out_local) {
  layout_fragment_t* containing_fragment;
  layout_point_t containing_local;
  layout_oof_positioned_t* containing_oof;
  lxb_status_t status;

  if (result == NULL || oof == NULL || oof->fragment == NULL || out_local == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (!layout_fragment_tree_can_traverse(oof->fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  containing_fragment = layout_result_oof_containing_fragment(result, oof);
  if (containing_fragment == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  containing_oof = layout_result_find_oof_for_fragment(result,
                                                       containing_fragment);
  if (containing_oof != NULL) {
    status = layout_result_root_point_to_oof_fragment(result,
                                                      containing_oof,
                                                      root_point,
                                                      coordinate_flags,
                                                      &containing_local);
  } else {
    if (!layout_fragment_path_can_traverse(result->root_fragment,
                                           containing_fragment)) {
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    status = layout_fragment_point_from_root(result->root_fragment,
                                             containing_fragment,
                                             root_point,
                                             coordinate_flags,
                                             &containing_local);
  }

  if (status != LXB_STATUS_OK) {
    return status;
  }

  containing_local.x -= oof->offset.x;
  containing_local.y -= oof->offset.y;

  if (!layout_fragment_ancestor_to_local(oof->fragment, containing_local, coordinate_flags, out_local)) {
    return LXB_STATUS_ERROR_UNEXPECTED_DATA;
  }

  return LXB_STATUS_OK;
}

lxb_status_t
layout_result_root_point_to_fragment(layout_result_t* result,
                                     layout_fragment_t* target_fragment,
                                     layout_point_t root_point,
                                     unsigned coordinate_flags,
                                     layout_point_t* out_local) {
  layout_oof_positioned_t* oof;
  lxb_status_t status;

  if (out_local == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_result_clean_fragment_read(result, target_fragment);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  oof = layout_result_find_oof_for_fragment(result, target_fragment);
  if (oof != NULL) {
    return layout_result_root_point_to_oof_fragment(result, oof, root_point, coordinate_flags, out_local);
  }

  if (!layout_fragment_path_can_traverse(result->root_fragment,
                                         target_fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  return layout_fragment_point_from_root(result->root_fragment,
                                         target_fragment,
                                         root_point,
                                         coordinate_flags,
                                         out_local);
}

lxb_status_t
layout_result_point_in_fragment(layout_result_t* result,
                                layout_fragment_t* target_fragment,
                                layout_point_t root_point,
                                layout_point_t* out_local) {
  return layout_result_root_point_to_fragment(
      result,
      target_fragment,
      root_point,
      LAYOUT_COORDINATE_APPLY_TRANSFORMS,
      out_local);
}

static lxb_status_t
layout_fragment_local_point_to_root(layout_fragment_t* root_fragment,
                                    layout_fragment_t* target_fragment,
                                    layout_point_t local_point,
                                    unsigned coordinate_flags,
                                    layout_point_t* out_root) {
  const layout_fragment_link_t* links[128];
  size_t length = 0;
  layout_point_t point = local_point;

  if (root_fragment == NULL || target_fragment == NULL || out_root == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (!layout_fragment_find_path(root_fragment, target_fragment, links, &length, sizeof(links) / sizeof(links[0]))) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  point = layout_fragment_local_to_ancestor(target_fragment, point, coordinate_flags);

  for (size_t i = length; i != 0; i--) {
    const layout_fragment_link_t* link = links[i - 1];
    layout_fragment_t* parent =
        i == 1 ? root_fragment : links[i - 2]->fragment;

    point.x += link->offset.x;
    point.y += link->offset.y;
    point = layout_fragment_local_to_ancestor(parent, point, coordinate_flags);
  }

  if (target_fragment == root_fragment) {
    point = layout_fragment_local_to_ancestor(root_fragment, point, coordinate_flags);
  }

  *out_root = point;
  return LXB_STATUS_OK;
}

static lxb_status_t
layout_result_oof_local_point_to_root(layout_result_t* result,
                                      layout_oof_positioned_t* oof,
                                      layout_point_t local_point,
                                      unsigned coordinate_flags,
                                      layout_point_t* out_root) {
  layout_fragment_t* containing_fragment;
  layout_oof_positioned_t* containing_oof;
  layout_point_t point;

  if (result == NULL || oof == NULL || oof->fragment == NULL || out_root == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  point = layout_fragment_local_to_ancestor(oof->fragment, local_point, coordinate_flags);
  point.x += oof->offset.x;
  point.y += oof->offset.y;

  containing_fragment = layout_result_oof_containing_fragment(result, oof);
  if (containing_fragment == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  containing_oof = layout_result_find_oof_for_fragment(result,
                                                       containing_fragment);
  if (containing_oof != NULL) {
    return layout_result_oof_local_point_to_root(result, containing_oof, point, coordinate_flags, out_root);
  }

  if (!layout_fragment_path_can_traverse(result->root_fragment,
                                         containing_fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  return layout_fragment_local_point_to_root(result->root_fragment,
                                             containing_fragment,
                                             point,
                                             coordinate_flags,
                                             out_root);
}

lxb_status_t
layout_result_fragment_rect_to_root(layout_result_t* result,
                                    layout_fragment_t* target_fragment,
                                    layout_rect_t local_rect,
                                    unsigned coordinate_flags,
                                    layout_rect_t* out_root_rect) {
  layout_point_t points[4];
  layout_oof_positioned_t* oof;
  lxb_status_t status;

  if (out_root_rect == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_result_clean_fragment_read(result, target_fragment);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  oof = layout_result_find_oof_for_fragment(result, target_fragment);
  if (oof != NULL && !layout_fragment_tree_can_traverse(oof->fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (oof == NULL && !layout_fragment_path_can_traverse(result->root_fragment,
                                                        target_fragment)) {
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  points[0].x = local_rect.x;
  points[0].y = local_rect.y;
  points[1].x = local_rect.x + local_rect.width;
  points[1].y = local_rect.y;
  points[2].x = local_rect.x + local_rect.width;
  points[2].y = local_rect.y + local_rect.height;
  points[3].x = local_rect.x;
  points[3].y = local_rect.y + local_rect.height;

  for (size_t i = 0; i < 4; i++) {
    if (oof != NULL) {
      status = layout_result_oof_local_point_to_root(result, oof, points[i], coordinate_flags, &points[i]);
    } else {
      status = layout_fragment_local_point_to_root(result->root_fragment,
                                                   target_fragment,
                                                   points[i],
                                                   coordinate_flags,
                                                   &points[i]);
    }
    if (status != LXB_STATUS_OK) {
      return status;
    }
  }

  *out_root_rect = layout_rect_from_points(points, 4);
  return LXB_STATUS_OK;
}

lxb_status_t
layout_result_root_point_to_object(layout_result_t* result,
                                   layout_object_t* target_object,
                                   size_t fragment_index,
                                   layout_point_t root_point,
                                   unsigned coordinate_flags,
                                   layout_point_t* out_local,
                                   layout_fragment_t** out_fragment) {
  layout_fragment_t* fragment;
  lxb_status_t status;

  if (target_object == NULL || out_local == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (out_fragment != NULL) {
    *out_fragment = NULL;
  }

  if (result == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  status = layout_result_clean_fragment_read(result, result->root_fragment);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  fragment = layout_result_fragment_at_object(result->root_fragment,
                                              target_object,
                                              &fragment_index);
  if (fragment == NULL) {
    fragment = layout_result_oof_fragment_at_object(result, target_object, &fragment_index);
  }

  if (fragment == NULL) {
    return LXB_STATUS_ERROR_NOT_EXISTS;
  }

  status = layout_result_root_point_to_fragment(result, fragment, root_point, coordinate_flags, out_local);
  if (status != LXB_STATUS_OK) {
    return status;
  }

  if (out_fragment != NULL) {
    *out_fragment = fragment;
  }

  return LXB_STATUS_OK;
}

lxb_status_t
layout_hit_test(layout_result_t* result, layout_fragment_t* root_fragment,
                layout_point_t root_point, layout_fragment_t** out_fragment) {
  layout_fragment_t* hit;

  if (result == NULL || out_fragment == NULL) {
    return LXB_STATUS_ERROR_OBJECT_IS_NULL;
  }

  if (root_fragment == NULL) {
    root_fragment = result->root_fragment;
  }

  if (root_fragment == NULL) {
    *out_fragment = NULL;
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (root_fragment->result != result) {
    *out_fragment = NULL;
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  if (!result->frozen) {
    *out_fragment = NULL;
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (layout_result_or_root_needs_layout(result, root_fragment)) {
    *out_fragment = NULL;
    return LXB_STATUS_ERROR_WRONG_STAGE;
  }

  if (!layout_fragment_tree_can_traverse(root_fragment)) {
    *out_fragment = NULL;
    return LXB_STATUS_ERROR_WRONG_ARGS;
  }

  for (size_t i = 0; i < result->oofs_length; i++) {
    if (!layout_fragment_tree_can_traverse(result->oofs[i].fragment)) {
      *out_fragment = NULL;
      return LXB_STATUS_ERROR_WRONG_ARGS;
    }
  }

  hit = layout_hit_test_fragment(result, root_fragment, root_point);
  *out_fragment = hit;

  return hit == NULL ? LXB_STATUS_ERROR_NOT_EXISTS : LXB_STATUS_OK;
}
