#ifndef LAYOUT_INTERNAL_H
#define LAYOUT_INTERNAL_H

#include "layout/layout.h"

#include "lexbor/core/dobject.h"
#include "lexbor/core/mraw.h"

#define LAYOUT_OBJECT_CHUNK 128
#define LAYOUT_BOX_CHUNK 128
#define LAYOUT_BLOCK_CHUNK 128
#define LAYOUT_FRAGMENT_CHUNK 128
#define LAYOUT_TREE_NODE_CHUNK 128
#define LAYOUT_SCENE_NODE_CHUNK 128
#define LAYOUT_SCENE_PATCH_CHUNK 128
#define LAYOUT_ARENA_CHUNK 4096

#define LAYOUT_FRAGMENT_DATA_IS_FIRST 1u
#define LAYOUT_FRAGMENT_DATA_HAS_PAINT_OFFSET 2u
#define LAYOUT_OBJECT_INTERNAL_VIEWPORT_CONTAINING_BLOCK (1u << 30)
#define LAYOUT_OBJECT_INTERNAL_BLOCK (1u << 29)
#define LAYOUT_OBJECT_INTERNAL_ANONYMOUS_BLOCK (1u << 28)
#define LAYOUT_OBJECT_INTERNAL_TEXT (1u << 27)
#define LAYOUT_OBJECT_INTERNAL_BOX (1u << 26)
#define LAYOUT_IDENTITY_SALT 0x9e3779b97f4a7c15ull
#define LAYOUT_ANONYMOUS_IDENTITY_SALT 0xbf58476d1ce4e5b9ull
#define LAYOUT_GENERATION_SALT 0x94d049bb133111ebull

typedef struct layout_fragment_index_counter {
  layout_object_t* object;
  uint32_t next_index;
} layout_fragment_index_counter_t;

struct layout {
  lexbor_dobject_t* objects;
  lexbor_dobject_t* boxes;
  lexbor_dobject_t* blocks;
  uint64_t identity_salt;
  uint64_t anonymous_identity_salt;
  uint64_t generation_salt;
  layout_lifecycle_state_t lifecycle_state;
  unsigned detach_count;
  unsigned disallow_transition_count;
  bool lifecycle_postponed;
};

struct layout_result {
  layout_t* layout;
  lexbor_mraw_t* arena;
  lexbor_dobject_t* fragments;
  layout_fragment_t* root_fragment;
  layout_oof_positioned_t* oofs;
  size_t oofs_length;
  size_t oofs_capacity;
  layout_fragment_index_counter_t* fragment_index_counters;
  size_t fragment_index_counters_length;
  size_t fragment_index_counters_capacity;
  uint64_t generation;
  size_t ref_count;
  bool self_allocated;
  bool destroy_self_on_zero;
  bool frozen;
};

struct layout_box {
  layout_object_t* object;
  layout_result_t** layout_results;
  size_t layout_results_length;
  size_t layout_results_capacity;
};

typedef struct layout_block {
  layout_box_t* box;
  layout_object_child_list_t children;
} layout_block_t;

typedef struct layout_tree_node {
  lxb_dom_node_t* node;
  layout_object_t* object;
  layout_block_t* block;
} layout_tree_node_t;

struct layout_tree {
  layout_t* layout;
  lexbor_dobject_t* nodes;
  layout_object_t* root_object;
};

struct layout_oof_positioned {
  layout_object_t* object;
  layout_object_t* containing_block;
  layout_fragment_t* containing_fragment;
  layout_fragment_t* fragment;
  layout_point_t offset;
  int stacking_order;
  layout_fragment_placement_t placement;
  bool fixed_position;
};

struct layout_fragment_data {
  unsigned flags;
  layout_point_t paint_offset;
};

struct layout_fragment_data_list {
  layout_fragment_data_t first;
  size_t length;
};

struct layout_object {
  layout_t* layout;
  uint64_t id;
  layout_object_t* parent;
  layout_object_t* prev;
  layout_object_t* next;
  lxb_dom_node_t* node;
  const lxb_style_computed_t* style;
  layout_box_t* box;
  layout_fragment_data_list_t fragment_data;
  layout_native_embed_token_t native_embed_token;
  unsigned bitfields;
  unsigned dirty_bits;
};

struct layout_fragment {
  layout_result_t* result;
  layout_object_t* object;
  layout_fragment_link_t* children;
  size_t children_length;
  size_t children_capacity;
  layout_size_t size;
  layout_fragment_type_t type;
  layout_fragment_box_type_t box_type;
  layout_fragment_role_t role;
  layout_fragment_key_t key;
  unsigned flags;
  int stacking_order;
  layout_transform_t transform;
  layout_box_edges_t margin;
  layout_box_edges_t border;
  layout_box_edges_t padding;
  unsigned border_sides;
};

struct layout_fragment_link {
  layout_fragment_t* fragment;
  layout_point_t offset;
  int stacking_order;
  size_t sequence;
  layout_fragment_placement_t placement;
};

struct layout_scene_node {
  layout_fragment_key_t key;
  layout_fragment_key_t parent_key;
  layout_fragment_key_t previous_sibling_key;
  size_t index;
  size_t parent_index;
  size_t first_child_index;
  size_t last_child_index;
  size_t previous_sibling_index;
  size_t next_sibling_index;
  size_t child_count;
  size_t child_slot;
  layout_point_t offset;
  layout_rect_t local_rect;
  layout_transform_t transform;
  layout_overflow_clip_t clip_axes;
  layout_rect_t clip_rect;
  double opacity;
  unsigned flags;
  unsigned layer_hints;
  unsigned dirty_bits;
  layout_fragment_placement_t placement;
  size_t sequence;
  int stacking_order;
  layout_native_embed_token_t native_embed_token;
};

struct layout_scene_plan {
  layout_t* layout;
  layout_result_t* result;
  layout_scene_node_t* nodes;
  size_t length;
  size_t capacity;
  uint64_t generation;
};

struct layout_scene_patch {
  layout_scene_patch_op_t op;
  layout_fragment_key_t key;
  layout_fragment_key_t old_parent_key;
  layout_fragment_key_t new_parent_key;
  layout_fragment_key_t old_previous_sibling_key;
  layout_fragment_key_t new_previous_sibling_key;
  size_t old_index;
  size_t new_index;
  size_t old_parent_index;
  size_t new_parent_index;
  size_t old_slot;
  size_t new_slot;
  size_t old_previous_sibling_index;
  size_t new_previous_sibling_index;
  size_t old_sequence;
  size_t new_sequence;
  layout_fragment_placement_t old_placement;
  layout_fragment_placement_t new_placement;
  int old_stacking_order;
  int new_stacking_order;
  unsigned dirty_bits;
};

struct layout_scene_diff {
  layout_scene_patch_t* patches;
  size_t length;
  size_t capacity;
  uint64_t old_generation;
  uint64_t new_generation;
};

layout_tree_node_t*
layout_tree_node_for_object(layout_tree_t* tree, layout_object_t* object);

layout_tree_node_t*
layout_tree_node_for_dom_node(layout_tree_t* tree, lxb_dom_node_t* dom_node);

layout_tree_node_t*
layout_tree_append_record(layout_tree_t* tree, lxb_dom_node_t* dom_node,
                          layout_object_t* object);

lxb_status_t
layout_tree_builder_attach_subtree(layout_tree_t* tree, lxb_dom_node_t* node,
                                   layout_tree_node_t* parent,
                                   bool reject_existing_record);

lxb_dom_node_t*
layout_tree_traversal_parent(lxb_dom_node_t* node);

lxb_dom_node_t*
layout_tree_traversal_layout_parent(lxb_dom_node_t* node);

lxb_dom_node_t*
layout_tree_traversal_first_child(lxb_dom_node_t* node);

lxb_dom_node_t*
layout_tree_traversal_next_sibling(lxb_dom_node_t* node);

bool
layout_tree_traversal_node_is_display_none(lxb_dom_node_t* node);

bool
layout_tree_traversal_node_is_display_contents(lxb_dom_node_t* node);

lxb_dom_node_t*
layout_tree_traversal_first_layout_child(lxb_dom_node_t* node);

lxb_dom_node_t*
layout_tree_traversal_next_layout_sibling(lxb_dom_node_t* node);

layout_object_t*
layout_dom_node_create_layout_object(layout_t* layout, lxb_dom_node_t* node,
                                     const lxb_style_computed_t* style,
                                     unsigned internal_bits);

void
layout_tree_prepare_dom_node_for_attach(lxb_dom_node_t* node);

void
layout_object_update_style_derived_bits(layout_object_t* object);

void
layout_object_unlink_from_siblings(layout_object_t* object);

void
layout_object_init_fragment_data(layout_object_t* object);

layout_box_t*
layout_box_create(layout_t* layout, layout_object_t* object);

void
layout_box_release_all_results(layout_t* layout);

void
layout_object_detach_box(layout_object_t* object);

void
layout_object_set_internal_type_bits(layout_object_t* object,
                                     unsigned internal_bits);

lxb_status_t
layout_lifecycle_mark_update_pending(layout_t* layout);

void
layout_lifecycle_abort_update(layout_t* layout);

#endif /* LAYOUT_INTERNAL_H */
