/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#ifndef LEXBOR_STYLE_PROPERTY_COMPUTE_H
#define LEXBOR_STYLE_PROPERTY_COMPUTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lexbor/style/resolver.h"


typedef void (*lxb_style_property_compute_f)(void *ctx);


LXB_API lxb_style_property_compute_f
lxb_style_property_compute_by_id(lxb_css_property_type_t id);

LXB_API void lxb_css_property_compute_background_color(void *ctx);
LXB_API void lxb_css_property_compute_border(void *ctx);
LXB_API void lxb_css_property_compute_border_bottom(void *ctx);
LXB_API void lxb_css_property_compute_border_bottom_color(void *ctx);
LXB_API void lxb_css_property_compute_border_left(void *ctx);
LXB_API void lxb_css_property_compute_border_left_color(void *ctx);
LXB_API void lxb_css_property_compute_border_right(void *ctx);
LXB_API void lxb_css_property_compute_border_right_color(void *ctx);
LXB_API void lxb_css_property_compute_border_top(void *ctx);
LXB_API void lxb_css_property_compute_border_top_color(void *ctx);
LXB_API void lxb_css_property_compute_bottom(void *ctx);
LXB_API void lxb_css_property_compute_box_sizing(void *ctx);
LXB_API void lxb_css_property_compute_clear(void *ctx);
LXB_API void lxb_css_property_compute_color(void *ctx);
LXB_API void lxb_css_property_compute_direction(void *ctx);
LXB_API void lxb_css_property_compute_display(void *ctx);
LXB_API void lxb_css_property_compute_float(void *ctx);
LXB_API void lxb_css_property_compute_font_size(void *ctx);
LXB_API void lxb_css_property_compute_font_stretch(void *ctx);
LXB_API void lxb_css_property_compute_font_style(void *ctx);
LXB_API void lxb_css_property_compute_font_weight(void *ctx);
LXB_API void lxb_css_property_compute_height(void *ctx);
LXB_API void lxb_css_property_compute_left(void *ctx);
LXB_API void lxb_css_property_compute_line_height(void *ctx);
LXB_API void lxb_css_property_compute_margin(void *ctx);
LXB_API void lxb_css_property_compute_margin_bottom(void *ctx);
LXB_API void lxb_css_property_compute_margin_left(void *ctx);
LXB_API void lxb_css_property_compute_margin_right(void *ctx);
LXB_API void lxb_css_property_compute_margin_top(void *ctx);
LXB_API void lxb_css_property_compute_max_height(void *ctx);
LXB_API void lxb_css_property_compute_max_width(void *ctx);
LXB_API void lxb_css_property_compute_min_height(void *ctx);
LXB_API void lxb_css_property_compute_min_width(void *ctx);
LXB_API void lxb_css_property_compute_opacity(void *ctx);
LXB_API void lxb_css_property_compute_overflow_x(void *ctx);
LXB_API void lxb_css_property_compute_overflow_y(void *ctx);
LXB_API void lxb_css_property_compute_padding(void *ctx);
LXB_API void lxb_css_property_compute_padding_bottom(void *ctx);
LXB_API void lxb_css_property_compute_padding_left(void *ctx);
LXB_API void lxb_css_property_compute_padding_right(void *ctx);
LXB_API void lxb_css_property_compute_padding_top(void *ctx);
LXB_API void lxb_css_property_compute_position(void *ctx);
LXB_API void lxb_css_property_compute_right(void *ctx);
LXB_API void lxb_css_property_compute_text_align(void *ctx);
LXB_API void lxb_css_property_compute_top(void *ctx);
LXB_API void lxb_css_property_compute_visibility(void *ctx);
LXB_API void lxb_css_property_compute_white_space(void *ctx);
LXB_API void lxb_css_property_compute_width(void *ctx);
LXB_API void lxb_css_property_compute_writing_mode(void *ctx);
LXB_API void lxb_css_property_compute_z_index(void *ctx);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_STYLE_PROPERTY_COMPUTE_H */
