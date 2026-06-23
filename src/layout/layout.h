#ifndef LAYOUT_H
#define LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lexbor/core/base.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/node.h"
#include "lexbor/style/computed.h"

typedef struct layout layout_t;
typedef struct layout_result layout_result_t;
typedef struct layout_tree layout_tree_t;
typedef struct layout_object layout_object_t;
typedef struct layout_fragment layout_fragment_t;
typedef struct layout_fragment_link layout_fragment_link_t;
typedef struct layout_fragment_data layout_fragment_data_t;
typedef struct layout_fragment_data_list layout_fragment_data_list_t;
typedef struct layout_oof_positioned layout_oof_positioned_t;
typedef struct layout_scene_plan layout_scene_plan_t;
typedef struct layout_scene_node layout_scene_node_t;
typedef struct layout_scene_diff layout_scene_diff_t;
typedef struct layout_scene_patch layout_scene_patch_t;

typedef size_t layout_native_embed_token_t;

#define LAYOUT_SCENE_NODE_NONE ((size_t)-1)

typedef enum {
  LAYOUT_OVERFLOW_CLIP_NONE = 0,
  LAYOUT_OVERFLOW_CLIP_X = 1 << 0,
  LAYOUT_OVERFLOW_CLIP_Y = 1 << 1,
  LAYOUT_OVERFLOW_CLIP_BOTH = LAYOUT_OVERFLOW_CLIP_X | LAYOUT_OVERFLOW_CLIP_Y
} layout_overflow_clip_t;

typedef enum {
  LAYOUT_DIRTY_NONE = 0,
  LAYOUT_DIRTY_SELF_FULL = 1 << 0,
  LAYOUT_DIRTY_CHILD_FULL = 1 << 1,
  LAYOUT_DIRTY_SIMPLIFIED = 1 << 2
} layout_dirty_t;

typedef struct {
  double x;
  double y;
} layout_point_t;

typedef struct {
  double width;
  double height;
} layout_size_t;

typedef struct {
  double x;
  double y;
  double width;
  double height;
} layout_rect_t;

typedef struct {
  double top;
  double right;
  double bottom;
  double left;
} layout_box_edges_t;

typedef struct {
  double a;
  double b;
  double c;
  double d;
  double e;
  double f;
} layout_transform_t;

typedef enum {
  LAYOUT_BOX_SIDE_NONE = 0,
  LAYOUT_BOX_SIDE_TOP = 1 << 0,
  LAYOUT_BOX_SIDE_RIGHT = 1 << 1,
  LAYOUT_BOX_SIDE_BOTTOM = 1 << 2,
  LAYOUT_BOX_SIDE_LEFT = 1 << 3,
  LAYOUT_BOX_SIDE_ALL = LAYOUT_BOX_SIDE_TOP | LAYOUT_BOX_SIDE_RIGHT | LAYOUT_BOX_SIDE_BOTTOM | LAYOUT_BOX_SIDE_LEFT
} layout_box_side_t;

typedef enum {
  LAYOUT_COORDINATE_APPLY_TRANSFORMS = 0,
  LAYOUT_COORDINATE_IGNORE_TRANSFORMS = 1 << 0,
  LAYOUT_COORDINATE_FLATTEN_3D = 1 << 1
} layout_coordinate_flags_t;

typedef enum {
  LAYOUT_OBJECT_ANONYMOUS = 1 << 0,
  LAYOUT_OBJECT_POINTER_EVENTS_NONE = 1 << 1,
  LAYOUT_OBJECT_CAN_CONTAIN_ABSOLUTE_POSITION = 1 << 2,
  LAYOUT_OBJECT_CAN_CONTAIN_FIXED_POSITION = 1 << 3,
  LAYOUT_OBJECT_COLUMN_SPANNER_CONTAINER = 1 << 4,
  LAYOUT_OBJECT_CAN_TRAVERSE_PHYSICAL_FRAGMENTS = 1 << 5,
  LAYOUT_OBJECT_COLUMN_SPANNER = 1 << 6,
  LAYOUT_OBJECT_HAS_CLIP_PATH = 1 << 7
} layout_object_bit_t;

typedef enum {
  LAYOUT_FRAGMENT_BOX = 0,
  LAYOUT_FRAGMENT_LINE_BOX = 1
} layout_fragment_type_t;

typedef enum {
  LAYOUT_FRAGMENT_BOX_NORMAL = 0,
  LAYOUT_FRAGMENT_BOX_INLINE,
  LAYOUT_FRAGMENT_BOX_COLUMN,
  LAYOUT_FRAGMENT_BOX_ATOMIC_INLINE,
  LAYOUT_FRAGMENT_BOX_FLOATING,
  LAYOUT_FRAGMENT_BOX_OUT_OF_FLOW_POSITIONED,
  LAYOUT_FRAGMENT_BOX_BLOCK_FLOW_ROOT
} layout_fragment_box_type_t;

typedef enum {
  LAYOUT_FRAGMENT_HIDDEN_FOR_PAINT = 1 << 0,
  LAYOUT_FRAGMENT_OPAQUE = 1 << 1,
  LAYOUT_FRAGMENT_CHILDREN_VALID = 1 << 2
} layout_fragment_flags_t;

typedef enum {
  LAYOUT_FRAGMENT_ROLE_AUTO = 0,
  LAYOUT_FRAGMENT_ROLE_NORMAL,
  LAYOUT_FRAGMENT_ROLE_ANONYMOUS,
  LAYOUT_FRAGMENT_ROLE_OUT_OF_FLOW,
  LAYOUT_FRAGMENT_ROLE_NATIVE_EMBED,
  LAYOUT_FRAGMENT_ROLE_PSEUDO,
  LAYOUT_FRAGMENT_ROLE_FRAGMENTAINER
} layout_fragment_role_t;

typedef enum {
  LAYOUT_FRAGMENT_PLACEMENT_NORMAL = 0,
  LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW,
  LAYOUT_FRAGMENT_PLACEMENT_FLOAT,
  LAYOUT_FRAGMENT_PLACEMENT_SCROLL_CONTENTS,
  LAYOUT_FRAGMENT_PLACEMENT_OVERLAY
} layout_fragment_placement_t;

typedef enum {
  LAYOUT_SCENE_NODE_VISIBLE = 1 << 0,
  LAYOUT_SCENE_NODE_HIDDEN_FOR_PAINT = 1 << 1,
  LAYOUT_SCENE_NODE_POINTER_EVENTS_NONE = 1 << 2,
  LAYOUT_SCENE_NODE_OPAQUE = 1 << 3,
  LAYOUT_SCENE_NODE_HAS_CLIP_PATH = 1 << 4,
  LAYOUT_SCENE_NODE_NATIVE_EMBED = 1 << 5,
  LAYOUT_SCENE_NODE_STACKING_CONTEXT = 1 << 6
} layout_scene_node_flags_t;

typedef enum {
  LAYOUT_SCENE_LAYER_HINT_NONE = 0,
  LAYOUT_SCENE_LAYER_HINT_TRANSFORM = 1 << 0,
  LAYOUT_SCENE_LAYER_HINT_CLIP = 1 << 1,
  LAYOUT_SCENE_LAYER_HINT_OPACITY = 1 << 2,
  LAYOUT_SCENE_LAYER_HINT_NATIVE_EMBED = 1 << 3,
  LAYOUT_SCENE_LAYER_HINT_OPAQUE = 1 << 4,
  LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY = 1 << 5
} layout_scene_layer_hint_t;

typedef enum {
  LAYOUT_SCENE_DIRTY_NONE = 0,
  LAYOUT_SCENE_DIRTY_LAYOUT = 1 << 0,
  LAYOUT_SCENE_DIRTY_GEOMETRY = 1 << 1,
  LAYOUT_SCENE_DIRTY_TRANSFORM = 1 << 2,
  LAYOUT_SCENE_DIRTY_CLIP = 1 << 3,
  LAYOUT_SCENE_DIRTY_OPACITY = 1 << 4,
  LAYOUT_SCENE_DIRTY_PAINT = 1 << 5,
  LAYOUT_SCENE_DIRTY_COMPOSITING = 1 << 6,
  LAYOUT_SCENE_DIRTY_CHILD_LIST = 1 << 7,
  LAYOUT_SCENE_DIRTY_NATIVE_EMBED = 1 << 8
} layout_scene_dirty_t;

typedef enum {
  LAYOUT_SCENE_PATCH_KEEP = 0,
  LAYOUT_SCENE_PATCH_INSERT,
  LAYOUT_SCENE_PATCH_REMOVE,
  LAYOUT_SCENE_PATCH_MOVE,
  LAYOUT_SCENE_PATCH_UPDATE
} layout_scene_patch_op_t;

typedef struct {
  uint64_t object_id;
  uint32_t fragment_index;
  uint32_t role;
  uint32_t ordinal;
} layout_fragment_key_t;

typedef struct {
  layout_object_t* object;
  layout_size_t size;
  layout_fragment_type_t type;
  layout_fragment_box_type_t box_type;
  layout_fragment_role_t role;
  unsigned flags;
  int stacking_order;
  layout_transform_t transform;
  layout_box_edges_t margin;
  layout_box_edges_t border;
  layout_box_edges_t padding;
  unsigned border_sides;
} layout_fragment_init_t;

typedef struct {
  layout_size_t viewport_size;
  layout_size_t default_object_size;
  double default_block_gap;
} layout_fragment_builder_t;

typedef struct layout_object_child_list {
  layout_object_t* first_child;
  layout_object_t* last_child;
} layout_object_child_list_t;

typedef struct {
  layout_object_t* last;
  layout_object_t* other_last;
} layout_object_common_ancestor_data_t;

typedef struct {
  lxb_css_display_type_t display_box;
  lxb_css_display_type_t display_outside;
  lxb_css_display_type_t display_inside;
  lxb_css_position_type_t position;
  lxb_css_overflow_x_type_t overflow_x;
  lxb_css_overflow_y_type_t overflow_y;
  lxb_css_visibility_type_t visibility;
  int z_index;
  bool has_z_index;
  bool has_transform;
  layout_transform_t transform;
} layout_object_state_t;

#define layout_dom_node_layout_object(node) \
  ((layout_object_t*)((node)->layout_object))

layout_t*
layout_create(void);

lxb_status_t
layout_init(layout_t* layout);

layout_t*
layout_destroy(layout_t* layout, bool destroy_self);

void layout_clean(layout_t* layout);

layout_tree_t*
layout_tree_create(layout_t* layout);

lxb_status_t
layout_tree_init(layout_tree_t* tree, layout_t* layout);

layout_tree_t*
layout_tree_destroy(layout_tree_t* tree, bool destroy_self);

void layout_tree_clean(layout_tree_t* tree);

layout_t*
layout_tree_layout(layout_tree_t* tree);

lxb_status_t
layout_tree_build(layout_tree_t* tree, lxb_dom_node_t* root_node);

lxb_status_t
layout_dom_node_ensure_layout_object(layout_t* layout, lxb_dom_node_t* node,
                                     layout_object_t** out_object);

void layout_dom_node_detach_layout_tree(lxb_dom_node_t* node);

void layout_dom_node_detach_layout_subtree(lxb_dom_node_t* root_node);

layout_object_t*
layout_tree_root_object(layout_tree_t* tree);

layout_object_t*
layout_tree_object_for_node(layout_tree_t* tree, lxb_dom_node_t* node);

layout_object_child_list_t*
layout_tree_children(layout_tree_t* tree, layout_object_t* object);

layout_object_t*
layout_tree_object_next_in_preorder(layout_tree_t* tree,
                                    layout_object_t* object,
                                    layout_object_t* stay_within);

layout_object_t*
layout_tree_object_next_in_preorder_after_children(layout_tree_t* tree,
                                                   layout_object_t* object,
                                                   layout_object_t* stay_within);

layout_object_t*
layout_tree_object_previous_in_preorder(layout_tree_t* tree,
                                        layout_object_t* object,
                                        layout_object_t* stay_within);

layout_object_t*
layout_tree_object_previous_in_postorder(layout_tree_t* tree,
                                         layout_object_t* object,
                                         layout_object_t* stay_within);

lxb_status_t
layout_tree_build_fragments(layout_tree_t* tree,
                            layout_result_t* result,
                            const layout_fragment_builder_t* builder);

layout_result_t*
layout_result_create(layout_t* layout);

lxb_status_t
layout_result_init(layout_result_t* result, layout_t* layout);

layout_result_t*
layout_result_destroy(layout_result_t* result, bool destroy_self);

void layout_result_clean(layout_result_t* result);

layout_t*
layout_result_layout(layout_result_t* result);

lxb_status_t
layout_result_freeze(layout_result_t* result);

bool layout_result_is_frozen(layout_result_t* result);

/*
 * Opaque snapshot identity token for retained scene-plan consumers.  Only
 * equality/inequality is meaningful; do not sort, persist, or assume monotonic
 * numeric growth.
 */
uint64_t
layout_result_generation(layout_result_t* result);

lxb_status_t
layout_result_set_root_fragment(layout_result_t* result,
                                layout_fragment_t* root_fragment);

layout_fragment_t*
layout_result_root_fragment(layout_result_t* result);

lxb_status_t
layout_result_add_out_of_flow_positioned(layout_result_t* result,
                                         layout_object_t* object,
                                         layout_object_t* containing_block,
                                         layout_fragment_t* fragment,
                                         layout_point_t offset,
                                         bool fixed_position);

lxb_status_t
layout_result_add_out_of_flow_positioned_in_fragment(
    layout_result_t* result,
    layout_object_t* object,
    layout_object_t* containing_block,
    layout_fragment_t* containing_fragment,
    layout_fragment_t* fragment,
    layout_point_t offset,
    bool fixed_position);

size_t
layout_result_out_of_flow_positioned_count(layout_result_t* result);

layout_oof_positioned_t*
layout_result_out_of_flow_positioned_at(layout_result_t* result,
                                        size_t index);

layout_object_t*
layout_oof_positioned_object(layout_oof_positioned_t* oof);

layout_object_t*
layout_oof_positioned_containing_block(layout_oof_positioned_t* oof);

layout_fragment_t*
layout_oof_positioned_containing_fragment(layout_oof_positioned_t* oof);

layout_fragment_t*
layout_oof_positioned_fragment(layout_oof_positioned_t* oof);

layout_point_t
layout_oof_positioned_offset(layout_oof_positioned_t* oof);

int layout_oof_positioned_stacking_order(layout_oof_positioned_t* oof);

bool layout_oof_positioned_is_fixed(layout_oof_positioned_t* oof);

layout_fragment_placement_t
layout_oof_positioned_placement(layout_oof_positioned_t* oof);

layout_object_t*
layout_object_create_anonymous(layout_t* layout,
                               const lxb_style_computed_t* style);

void layout_object_child_list_init(layout_object_child_list_t* children);

lxb_status_t
layout_object_set_parent(layout_object_t* object, layout_object_t* parent);

lxb_status_t
layout_object_set_previous_sibling(layout_object_t* object,
                                   layout_object_t* previous);

lxb_status_t
layout_object_set_next_sibling(layout_object_t* object,
                               layout_object_t* next);

lxb_dom_node_t*
layout_object_node(layout_object_t* object);

/*
 * Opaque layout-object identity token.  DOM-backed objects derive identity from
 * the DOM node inside the owning layout context.  Anonymous objects derive it
 * from their current object instance and are invalidated across layout_clean().
 */
uint64_t
layout_object_id(layout_object_t* object);

bool layout_object_is_anonymous(layout_object_t* object);

bool layout_object_can_have_children(layout_object_t* object);

lxb_status_t
layout_object_set_style(layout_object_t* object,
                        const lxb_style_computed_t* style);

const lxb_style_computed_t*
layout_object_style(layout_object_t* object);

void layout_object_set_pointer_events_none(layout_object_t* object, bool value);

bool layout_object_pointer_events_none(layout_object_t* object);

void layout_object_set_has_clip_path(layout_object_t* object, bool value);

bool layout_object_has_clip_path(layout_object_t* object);

void layout_object_set_can_contain_absolute_position(layout_object_t* object,
                                                     bool value);

bool layout_object_can_contain_absolute_position(layout_object_t* object);

void layout_object_set_can_contain_fixed_position(layout_object_t* object,
                                                  bool value);

bool layout_object_can_contain_fixed_position(layout_object_t* object);

void layout_object_set_column_spanner_container(layout_object_t* object,
                                                bool value);

bool layout_object_is_column_spanner_container(layout_object_t* object);

void layout_object_set_column_spanner(layout_object_t* object, bool value);

bool layout_object_is_column_spanner(layout_object_t* object);

void layout_object_set_can_traverse_physical_fragments(layout_object_t* object,
                                                       bool value);

bool layout_object_can_traverse_physical_fragments(layout_object_t* object);

/*
 * Token owned by the embedding layer for native controls.  The C layout tree
 * stores only this stable key; it must not store or own native UI object
 * pointers.
 */
void layout_object_set_native_embed_token(layout_object_t* object,
                                          layout_native_embed_token_t token);

layout_native_embed_token_t
layout_object_native_embed_token(layout_object_t* object);

bool layout_object_is_native_embed_host(layout_object_t* object);

layout_object_t*
layout_object_parent(layout_object_t* object);

layout_object_t*
layout_object_previous_sibling(layout_object_t* object);

layout_object_t*
layout_object_next_sibling(layout_object_t* object);

size_t
layout_object_depth(layout_object_t* object);

layout_object_t*
layout_object_common_ancestor(layout_object_t* object,
                              layout_object_t* other,
                              layout_object_common_ancestor_data_t* data);

bool layout_object_is_before_in_preorder(layout_object_t* object,
                                         layout_object_t* other);

layout_object_t*
layout_object_slow_first_child(layout_object_child_list_t* children);

layout_object_t*
layout_object_slow_last_child(layout_object_child_list_t* children);

lxb_status_t
layout_object_child_list_append(layout_object_child_list_t* children,
                                layout_object_t* parent,
                                layout_object_t* child);

lxb_status_t
layout_object_child_list_insert_before(layout_object_child_list_t* children,
                                       layout_object_t* parent,
                                       layout_object_t* child,
                                       layout_object_t* before_child);

lxb_status_t
layout_object_child_list_remove(layout_object_child_list_t* children,
                                layout_object_t* child);

lxb_status_t
layout_object_child_list_clear(layout_object_child_list_t* children,
                               layout_object_t* parent);

lxb_status_t
layout_object_state(layout_object_t* object, layout_object_state_t* out_state);

layout_fragment_data_list_t*
layout_object_fragment_data_list(layout_object_t* object);

layout_fragment_data_t*
layout_object_first_fragment_data(layout_object_t* object);

size_t
layout_object_fragment_data_count(layout_object_t* object);

bool layout_fragment_data_is_first(layout_fragment_data_t* fragment_data);

void layout_fragment_data_set_paint_offset(layout_fragment_data_t* fragment_data,
                                           layout_point_t paint_offset);

layout_point_t
layout_fragment_data_paint_offset(layout_fragment_data_t* fragment_data);

layout_object_t*
layout_object_containing_block_for_absolute(layout_object_t* object);

layout_object_t*
layout_object_containing_block_for_fixed(layout_object_t* object);

layout_object_t*
layout_object_containing_block_for_column_spanner(layout_object_t* object);

bool layout_object_can_contain_out_of_flow_positioned(layout_object_t* object,
                                                      bool fixed_position);

void layout_object_set_self_needs_full_layout(layout_object_t* object, bool value);

void layout_object_set_child_needs_full_layout(layout_object_t* object, bool value);

void layout_object_set_needs_simplified_layout(layout_object_t* object, bool value);

unsigned
layout_object_dirty_bits(layout_object_t* object);

bool layout_object_self_needs_full_layout(layout_object_t* object);

bool layout_object_child_needs_full_layout(layout_object_t* object);

bool layout_object_needs_simplified_layout(layout_object_t* object);

bool layout_object_subtree_needs_layout(layout_object_t* object);

void layout_object_clear_needs_layout(layout_object_t* object);

bool layout_result_needs_layout(layout_result_t* result);

layout_fragment_t*
layout_fragment_create(layout_result_t* result,
                       const layout_fragment_init_t* init);

layout_fragment_t*
layout_fragment_clone_subtree(layout_result_t* result,
                              layout_fragment_t* source_fragment);

lxb_status_t
layout_fragment_append_cloned_child(layout_fragment_t* parent,
                                    const layout_fragment_link_t* source_link,
                                    layout_point_t offset,
                                    layout_fragment_t** out_cloned_child);

lxb_status_t
layout_fragment_append_child(layout_fragment_t* parent,
                             layout_fragment_t* child,
                             layout_point_t offset);

lxb_status_t
layout_fragment_append_child_with_stacking_order(layout_fragment_t* parent,
                                                 layout_fragment_t* child,
                                                 layout_point_t offset,
                                                 int stacking_order);

lxb_status_t
layout_fragment_append_child_with_placement(
    layout_fragment_t* parent,
    layout_fragment_t* child,
    layout_point_t offset,
    layout_fragment_placement_t placement,
    int stacking_order);

int layout_fragment_stacking_order(layout_fragment_t* fragment);

layout_fragment_link_t*
layout_fragment_first_child_link(layout_fragment_t* fragment);

layout_fragment_link_t*
layout_fragment_last_child_link(layout_fragment_t* fragment);

size_t
layout_fragment_child_count(layout_fragment_t* fragment);

layout_fragment_link_t*
layout_fragment_child_link_at(layout_fragment_t* fragment, size_t index);

layout_fragment_t*
layout_fragment_link_fragment(layout_fragment_link_t* link);

layout_point_t
layout_fragment_link_offset(layout_fragment_link_t* link);

int layout_fragment_link_stacking_order(layout_fragment_link_t* link);

size_t
layout_fragment_link_sequence(layout_fragment_link_t* link);

layout_fragment_placement_t
layout_fragment_link_placement(layout_fragment_link_t* link);

layout_object_t*
layout_fragment_object(layout_fragment_t* fragment);

layout_fragment_key_t
layout_fragment_key(layout_fragment_t* fragment);

layout_fragment_role_t
layout_fragment_role(layout_fragment_t* fragment);

unsigned
layout_fragment_flags(layout_fragment_t* fragment);

bool layout_fragment_hidden_for_paint(layout_fragment_t* fragment);

bool layout_fragment_opaque(layout_fragment_t* fragment);

bool layout_fragment_children_valid(layout_fragment_t* fragment);

void layout_fragment_invalidate_children(layout_fragment_t* fragment);

layout_box_edges_t
layout_fragment_margin(layout_fragment_t* fragment);

layout_box_edges_t
layout_fragment_border(layout_fragment_t* fragment);

layout_box_edges_t
layout_fragment_padding(layout_fragment_t* fragment);

unsigned
layout_fragment_border_sides(layout_fragment_t* fragment);

layout_rect_t
layout_fragment_content_rect(layout_fragment_t* fragment);

lxb_status_t
layout_result_fragment_size(layout_result_t* result,
                            layout_fragment_t* fragment,
                            layout_size_t* out_size);

lxb_status_t
layout_result_fragment_local_rect(layout_result_t* result,
                                  layout_fragment_t* fragment,
                                  layout_rect_t* out_rect);

lxb_status_t
layout_result_fragment_link_offset(layout_result_t* result,
                                   layout_fragment_t* parent_fragment,
                                   size_t index,
                                   layout_point_t* out_offset);

lxb_status_t
layout_result_fragment_for_object(layout_result_t* result,
                                  layout_object_t* target_object,
                                  size_t fragment_index,
                                  layout_fragment_t** out_fragment);

layout_scene_plan_t*
layout_scene_plan_create(layout_t* layout);

lxb_status_t
layout_scene_plan_init(layout_scene_plan_t* plan, layout_t* layout);

layout_scene_plan_t*
layout_scene_plan_destroy(layout_scene_plan_t* plan, bool destroy_self);

void layout_scene_plan_clean(layout_scene_plan_t* plan);

layout_t*
layout_scene_plan_layout(layout_scene_plan_t* plan);

lxb_status_t
layout_scene_plan_build(layout_scene_plan_t* plan, layout_result_t* result);

uint64_t
layout_scene_plan_generation(layout_scene_plan_t* plan);

size_t
layout_scene_plan_node_count(layout_scene_plan_t* plan);

const layout_scene_node_t*
layout_scene_plan_node_at(layout_scene_plan_t* plan, size_t index);

size_t
layout_scene_node_index(const layout_scene_node_t* node);

size_t
layout_scene_node_parent_index(const layout_scene_node_t* node);

size_t
layout_scene_node_first_child_index(const layout_scene_node_t* node);

size_t
layout_scene_node_last_child_index(const layout_scene_node_t* node);

size_t
layout_scene_node_previous_sibling_index(const layout_scene_node_t* node);

size_t
layout_scene_node_next_sibling_index(const layout_scene_node_t* node);

size_t
layout_scene_node_child_count(const layout_scene_node_t* node);

layout_fragment_key_t
layout_scene_node_key(const layout_scene_node_t* node);

layout_fragment_key_t
layout_scene_node_parent_key(const layout_scene_node_t* node);

size_t
layout_scene_node_child_slot(const layout_scene_node_t* node);

layout_fragment_key_t
layout_scene_node_previous_sibling_key(const layout_scene_node_t* node);

layout_point_t
layout_scene_node_offset(const layout_scene_node_t* node);

layout_rect_t
layout_scene_node_local_rect(const layout_scene_node_t* node);

layout_transform_t
layout_scene_node_transform(const layout_scene_node_t* node);

layout_overflow_clip_t
layout_scene_node_clip_axes(const layout_scene_node_t* node);

layout_rect_t
layout_scene_node_clip_rect(const layout_scene_node_t* node);

double
layout_scene_node_opacity(const layout_scene_node_t* node);

unsigned
layout_scene_node_flags(const layout_scene_node_t* node);

unsigned
layout_scene_node_layer_hints(const layout_scene_node_t* node);

unsigned
layout_scene_node_dirty_bits(const layout_scene_node_t* node);

layout_fragment_placement_t
layout_scene_node_placement(const layout_scene_node_t* node);

size_t
layout_scene_node_sequence(const layout_scene_node_t* node);

int layout_scene_node_stacking_order(const layout_scene_node_t* node);

layout_native_embed_token_t
layout_scene_node_native_embed_token(const layout_scene_node_t* node);

layout_scene_diff_t*
layout_scene_diff_create(void);

lxb_status_t
layout_scene_diff_init(layout_scene_diff_t* diff);

layout_scene_diff_t*
layout_scene_diff_destroy(layout_scene_diff_t* diff, bool destroy_self);

void layout_scene_diff_clean(layout_scene_diff_t* diff);

lxb_status_t
layout_scene_plan_diff(layout_scene_plan_t* old_plan,
                       layout_scene_plan_t* new_plan,
                       layout_scene_diff_t* diff);

uint64_t
layout_scene_diff_old_generation(layout_scene_diff_t* diff);

uint64_t
layout_scene_diff_new_generation(layout_scene_diff_t* diff);

size_t
layout_scene_diff_patch_count(layout_scene_diff_t* diff);

const layout_scene_patch_t*
layout_scene_diff_patch_at(layout_scene_diff_t* diff, size_t index);

layout_scene_patch_op_t
layout_scene_patch_op(const layout_scene_patch_t* patch);

layout_fragment_key_t
layout_scene_patch_key(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_old_index(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_new_index(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_old_parent_index(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_new_parent_index(const layout_scene_patch_t* patch);

layout_fragment_key_t
layout_scene_patch_old_parent_key(const layout_scene_patch_t* patch);

layout_fragment_key_t
layout_scene_patch_new_parent_key(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_old_slot(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_new_slot(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_old_previous_sibling_index(
    const layout_scene_patch_t* patch);

size_t
layout_scene_patch_new_previous_sibling_index(
    const layout_scene_patch_t* patch);

layout_fragment_key_t
layout_scene_patch_old_previous_sibling_key(
    const layout_scene_patch_t* patch);

layout_fragment_key_t
layout_scene_patch_new_previous_sibling_key(
    const layout_scene_patch_t* patch);

size_t
layout_scene_patch_old_sequence(const layout_scene_patch_t* patch);

size_t
layout_scene_patch_new_sequence(const layout_scene_patch_t* patch);

layout_fragment_placement_t
layout_scene_patch_old_placement(const layout_scene_patch_t* patch);

layout_fragment_placement_t
layout_scene_patch_new_placement(const layout_scene_patch_t* patch);

int layout_scene_patch_old_stacking_order(const layout_scene_patch_t* patch);

int layout_scene_patch_new_stacking_order(const layout_scene_patch_t* patch);

unsigned
layout_scene_patch_dirty_bits(const layout_scene_patch_t* patch);

layout_overflow_clip_t
layout_object_overflow_clip(layout_object_t* object);

bool layout_transform_invert(const layout_transform_t* transform,
                             layout_transform_t* out);

layout_point_t
layout_transform_map_point(const layout_transform_t* transform,
                           layout_point_t point);

lxb_status_t
layout_result_point_in_fragment(layout_result_t* result,
                                layout_fragment_t* target_fragment,
                                layout_point_t root_point,
                                layout_point_t* out_local);

lxb_status_t
layout_result_root_point_to_fragment(layout_result_t* result,
                                     layout_fragment_t* target_fragment,
                                     layout_point_t root_point,
                                     unsigned coordinate_flags,
                                     layout_point_t* out_local);

lxb_status_t
layout_result_root_point_to_object(layout_result_t* result,
                                   layout_object_t* target_object,
                                   size_t fragment_index,
                                   layout_point_t root_point,
                                   unsigned coordinate_flags,
                                   layout_point_t* out_local,
                                   layout_fragment_t** out_fragment);

lxb_status_t
layout_result_fragment_rect_to_root(layout_result_t* result,
                                    layout_fragment_t* target_fragment,
                                    layout_rect_t local_rect,
                                    unsigned coordinate_flags,
                                    layout_rect_t* out_root_rect);

lxb_status_t
layout_hit_test(layout_result_t* result, layout_fragment_t* root_fragment,
                layout_point_t root_point, layout_fragment_t** out_fragment);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LAYOUT_H */
