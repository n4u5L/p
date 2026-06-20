#ifndef LEXBOR_STYLE_COMPUTED_H
#define LEXBOR_STYLE_COMPUTED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lexbor/style/base.h"

typedef enum {
  LXB_STYLE_COMPUTED_VALUE_UNDEF = 0x00,
  LXB_STYLE_COMPUTED_VALUE_AUTO,
  LXB_STYLE_COMPUTED_VALUE_NONE,
  LXB_STYLE_COMPUTED_VALUE_LENGTH,
  LXB_STYLE_COMPUTED_VALUE_PERCENTAGE,
  LXB_STYLE_COMPUTED_VALUE_NUMBER,
  LXB_STYLE_COMPUTED_VALUE_INTEGER,
  LXB_STYLE_COMPUTED_VALUE_KEYWORD
} lxb_style_computed_value_type_t;

typedef struct {
  lxb_style_computed_value_type_t type;
  union {
    struct {
      double num;
      lxb_css_unit_t unit;
    } length;

    double number;
    int32_t integer;
    uintptr_t keyword;
  } u;
} lxb_style_computed_value_t;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4201)
#endif

typedef struct {
  union {
    float rgba[4];
    struct {
      float r;
      float g;
      float b;
      float a;
    };
  };
} lxb_style_color_t;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

typedef enum {
  LXB_STYLE_EDGE_TOP = 0x00,
  LXB_STYLE_EDGE_RIGHT,
  LXB_STYLE_EDGE_BOTTOM,
  LXB_STYLE_EDGE_LEFT,
  LXB_STYLE_EDGE__LAST_ENTRY
} lxb_style_edge_t;

typedef struct {
  lxb_style_computed_value_t width;
  uintptr_t style;
  lxb_style_color_t color;
} lxb_style_computed_border_t;

typedef struct {
  size_t refs;

  lxb_css_text_align_type_t text_align;
  lxb_css_white_space_type_t white_space;
  lxb_css_writing_mode_type_t writing_mode;
} lxb_style_computed_rare_inherited_t;

typedef struct {
  size_t refs;

  lxb_style_color_t color;
  lxb_css_direction_type_t direction;
  lxb_css_visibility_type_t visibility;
  double font_size;
  double line_height;
  lxb_css_font_weight_type_t font_weight_type;
  double font_weight;
  lxb_css_font_style_type_t font_style;
  lxb_css_font_stretch_type_t font_stretch_type;
  double font_stretch;
  lxb_style_computed_rare_inherited_t* rare;
} lxb_style_computed_inherited_t;

typedef struct {
  size_t refs;

  lxb_css_display_type_t display_outside;
  lxb_css_display_type_t display_inside;
  bool display_list_item;
  lxb_css_display_type_t display_box;
  lxb_css_display_type_t display_internal;
  lxb_css_position_type_t position;
  lxb_css_box_sizing_type_t box_sizing;
  lxb_css_float_type_t floatp;
  lxb_css_clear_type_t clear;
  lxb_css_overflow_x_type_t overflow_x;
  lxb_css_overflow_y_type_t overflow_y;
  double opacity;
  bool z_index_auto;
  int32_t z_index;
} lxb_style_computed_non_inherited_t;

typedef struct {
  size_t refs;

  lxb_style_computed_value_t width;
  lxb_style_computed_value_t height;
  lxb_style_computed_value_t min_width;
  lxb_style_computed_value_t min_height;
  lxb_style_computed_value_t max_width;
  lxb_style_computed_value_t max_height;
  lxb_style_computed_value_t inset[LXB_STYLE_EDGE__LAST_ENTRY];
} lxb_style_computed_box_t;

typedef struct {
  size_t refs;

  lxb_style_computed_value_t margin[LXB_STYLE_EDGE__LAST_ENTRY];
  lxb_style_computed_value_t padding[LXB_STYLE_EDGE__LAST_ENTRY];
  lxb_style_computed_border_t border[LXB_STYLE_EDGE__LAST_ENTRY];
} lxb_style_computed_surround_t;

typedef struct {
  size_t refs;

  lxb_style_color_t background_color;
} lxb_style_computed_visual_t;

typedef struct {
  size_t refs;
  uintptr_t cache_id;
  size_t cache_generation;

  lxb_style_computed_inherited_t* inherited;
  lxb_style_computed_non_inherited_t* non_inherited;
  lxb_style_computed_box_t* box;
  lxb_style_computed_surround_t* surround;
  lxb_style_computed_visual_t* visual;
} lxb_style_computed_t;

LXB_API lxb_style_computed_t*
lxb_style_computed_create(void);

LXB_API lxb_style_computed_t*
lxb_style_computed_create_initial(const lxb_style_computed_t* initial,
                                  const lxb_style_computed_t* parent);

LXB_API void
lxb_style_computed_destroy(lxb_style_computed_t* style);

LXB_API void
lxb_style_computed_ref(lxb_style_computed_t* style);

LXB_API void
lxb_style_computed_unref(lxb_style_computed_t* style);

LXB_API bool
lxb_style_computed_equal(const lxb_style_computed_t* left,
                         const lxb_style_computed_t* right);

LXB_API lxb_status_t
lxb_style_computed_set_initial(lxb_style_computed_t* style,
                               double initial_font_size,
                               double initial_line_height);

/*
 * Mutable accessors detach only data groups. The caller must own the
 * top-level lxb_style_computed_t exclusively (style->refs == 1).
 */
LXB_API lxb_style_computed_inherited_t*
lxb_style_computed_inherited_mutable(lxb_style_computed_t* style);

LXB_API lxb_style_computed_rare_inherited_t*
lxb_style_computed_rare_inherited_mutable(lxb_style_computed_t* style);

LXB_API lxb_style_computed_non_inherited_t*
lxb_style_computed_non_inherited_mutable(lxb_style_computed_t* style);

LXB_API lxb_style_computed_box_t*
lxb_style_computed_box_mutable(lxb_style_computed_t* style);

LXB_API lxb_style_computed_surround_t*
lxb_style_computed_surround_mutable(lxb_style_computed_t* style);

LXB_API lxb_style_computed_visual_t*
lxb_style_computed_visual_mutable(lxb_style_computed_t* style);

LXB_API lxb_status_t
lxb_style_computed_detach_for_property(lxb_style_computed_t* style,
                                       lxb_css_property_type_t id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_STYLE_COMPUTED_H */
