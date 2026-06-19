/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

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
  lxb_css_unit_t unit;
  double num;
  uintptr_t keyword;
} lxb_style_computed_value_t;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} lxb_style_color_t;

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

typedef struct lxb_style_computed lxb_style_computed_t;
typedef struct lxb_style_computed_rare_inherited lxb_style_computed_rare_inherited_t;
typedef struct lxb_style_computed_inherited lxb_style_computed_inherited_t;
typedef struct lxb_style_computed_non_inherited lxb_style_computed_non_inherited_t;
typedef struct lxb_style_computed_box lxb_style_computed_box_t;
typedef struct lxb_style_computed_surround lxb_style_computed_surround_t;
typedef struct lxb_style_computed_visual lxb_style_computed_visual_t;

struct lxb_style_computed_rare_inherited {
  size_t refs;

  lxb_css_text_align_type_t text_align;
  lxb_css_white_space_type_t white_space;
  lxb_css_writing_mode_type_t writing_mode;
};

struct lxb_style_computed_inherited {
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
};

struct lxb_style_computed_non_inherited {
  size_t refs;

  lxb_css_display_type_t display_a;
  lxb_css_display_type_t display_b;
  lxb_css_display_type_t display_c;
  lxb_css_position_type_t position;
  lxb_css_box_sizing_type_t box_sizing;
  lxb_css_float_type_t floatp;
  lxb_css_clear_type_t clear;
  lxb_css_overflow_x_type_t overflow_x;
  lxb_css_overflow_y_type_t overflow_y;
  double opacity;
  bool z_index_auto;
  long z_index;
};

struct lxb_style_computed_box {
  size_t refs;

  lxb_style_computed_value_t width;
  lxb_style_computed_value_t height;
  lxb_style_computed_value_t min_width;
  lxb_style_computed_value_t min_height;
  lxb_style_computed_value_t max_width;
  lxb_style_computed_value_t max_height;
  lxb_style_computed_value_t inset[LXB_STYLE_EDGE__LAST_ENTRY];
};

struct lxb_style_computed_surround {
  size_t refs;

  lxb_style_computed_value_t margin[LXB_STYLE_EDGE__LAST_ENTRY];
  lxb_style_computed_value_t padding[LXB_STYLE_EDGE__LAST_ENTRY];
  lxb_style_computed_border_t border[LXB_STYLE_EDGE__LAST_ENTRY];
};

struct lxb_style_computed_visual {
  size_t refs;

  lxb_style_color_t background_color;
};

struct lxb_style_computed {
  size_t refs;

  lxb_style_computed_inherited_t* inherited;
  lxb_style_computed_non_inherited_t* non_inherited;
  lxb_style_computed_box_t* box;
  lxb_style_computed_surround_t* surround;
  lxb_style_computed_visual_t* visual;
};

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
                                       uintptr_t id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_STYLE_COMPUTED_H */
