/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/style/property_compute.h"
#include "lexbor/style/dom/interfaces/element.h"


typedef enum {
    LXB_STYLE_GLOBAL_NONE = 0x00,
    LXB_STYLE_GLOBAL_INITIAL,
    LXB_STYLE_GLOBAL_PARENT
}
lxb_style_global_t;


static lxb_style_global_t
lxb_style_compute_global(uintptr_t type, bool inherited,
                         const lxb_style_computed_t *parent);

static double
lxb_style_compute_length_px(const lxb_style_compute_ctx_t *ctx,
                            const lxb_css_value_length_t *length);

static void
lxb_style_compute_length_percentage(const lxb_style_compute_ctx_t *ctx,
                                    lxb_style_computed_value_t *dst,
                                    const lxb_css_value_length_percentage_t *src);

static void
lxb_style_compute_length_type(const lxb_style_compute_ctx_t *ctx,
                              lxb_style_computed_value_t *dst,
                              const lxb_css_value_length_type_t *src);

static void
lxb_style_compute_number_percentage(const lxb_css_value_number_percentage_t *src,
                                    double *dst);

static lxb_style_color_t
lxb_style_compute_color_value(const lxb_style_compute_ctx_t *ctx,
                              const lxb_css_value_color_t *src,
                              lxb_style_color_t current_color,
                              lxb_style_color_t initial);

static void
lxb_style_compute_edge_value(const lxb_style_compute_ctx_t *ctx,
                             lxb_style_computed_value_t *dst,
                             const lxb_css_value_length_percentage_t *src,
                             const lxb_style_computed_value_t *parent,
                             bool inherited,
                             lxb_style_computed_value_t initial);

static void
lxb_style_compute_border_value(const lxb_style_compute_ctx_t *ctx,
                               lxb_style_computed_border_t *dst,
                               const lxb_css_property_border_t *src,
                               const lxb_style_computed_border_t *parent);

static lxb_style_computed_value_t
lxb_style_compute_value_keyword(uintptr_t keyword);

static lxb_style_computed_value_t
lxb_style_compute_value_length(double px);

static double
lxb_style_compute_font_size_initial(const lxb_style_compute_ctx_t *ctx);

static double
lxb_style_compute_line_height_initial(const lxb_style_compute_ctx_t *ctx,
                                      double font_size);

static lxb_style_color_t
lxb_style_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);


lxb_style_property_compute_f
lxb_style_property_compute_by_id(lxb_css_property_type_t id)
{
    switch (id) {
        case LXB_CSS_PROPERTY_BACKGROUND_COLOR:
            return lxb_css_property_compute_background_color;
        case LXB_CSS_PROPERTY_BORDER:
            return lxb_css_property_compute_border;
        case LXB_CSS_PROPERTY_BORDER_BOTTOM:
            return lxb_css_property_compute_border_bottom;
        case LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
            return lxb_css_property_compute_border_bottom_color;
        case LXB_CSS_PROPERTY_BORDER_LEFT:
            return lxb_css_property_compute_border_left;
        case LXB_CSS_PROPERTY_BORDER_LEFT_COLOR:
            return lxb_css_property_compute_border_left_color;
        case LXB_CSS_PROPERTY_BORDER_RIGHT:
            return lxb_css_property_compute_border_right;
        case LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR:
            return lxb_css_property_compute_border_right_color;
        case LXB_CSS_PROPERTY_BORDER_TOP:
            return lxb_css_property_compute_border_top;
        case LXB_CSS_PROPERTY_BORDER_TOP_COLOR:
            return lxb_css_property_compute_border_top_color;
        case LXB_CSS_PROPERTY_BOTTOM:
            return lxb_css_property_compute_bottom;
        case LXB_CSS_PROPERTY_BOX_SIZING:
            return lxb_css_property_compute_box_sizing;
        case LXB_CSS_PROPERTY_CLEAR:
            return lxb_css_property_compute_clear;
        case LXB_CSS_PROPERTY_COLOR:
            return lxb_css_property_compute_color;
        case LXB_CSS_PROPERTY_DIRECTION:
            return lxb_css_property_compute_direction;
        case LXB_CSS_PROPERTY_DISPLAY:
            return lxb_css_property_compute_display;
        case LXB_CSS_PROPERTY_FLOAT:
            return lxb_css_property_compute_float;
        case LXB_CSS_PROPERTY_FONT_SIZE:
            return lxb_css_property_compute_font_size;
        case LXB_CSS_PROPERTY_FONT_STRETCH:
            return lxb_css_property_compute_font_stretch;
        case LXB_CSS_PROPERTY_FONT_STYLE:
            return lxb_css_property_compute_font_style;
        case LXB_CSS_PROPERTY_FONT_WEIGHT:
            return lxb_css_property_compute_font_weight;
        case LXB_CSS_PROPERTY_HEIGHT:
            return lxb_css_property_compute_height;
        case LXB_CSS_PROPERTY_LEFT:
            return lxb_css_property_compute_left;
        case LXB_CSS_PROPERTY_LINE_HEIGHT:
            return lxb_css_property_compute_line_height;
        case LXB_CSS_PROPERTY_MARGIN:
            return lxb_css_property_compute_margin;
        case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
            return lxb_css_property_compute_margin_bottom;
        case LXB_CSS_PROPERTY_MARGIN_LEFT:
            return lxb_css_property_compute_margin_left;
        case LXB_CSS_PROPERTY_MARGIN_RIGHT:
            return lxb_css_property_compute_margin_right;
        case LXB_CSS_PROPERTY_MARGIN_TOP:
            return lxb_css_property_compute_margin_top;
        case LXB_CSS_PROPERTY_MAX_HEIGHT:
            return lxb_css_property_compute_max_height;
        case LXB_CSS_PROPERTY_MAX_WIDTH:
            return lxb_css_property_compute_max_width;
        case LXB_CSS_PROPERTY_MIN_HEIGHT:
            return lxb_css_property_compute_min_height;
        case LXB_CSS_PROPERTY_MIN_WIDTH:
            return lxb_css_property_compute_min_width;
        case LXB_CSS_PROPERTY_OPACITY:
            return lxb_css_property_compute_opacity;
        case LXB_CSS_PROPERTY_OVERFLOW_X:
            return lxb_css_property_compute_overflow_x;
        case LXB_CSS_PROPERTY_OVERFLOW_Y:
            return lxb_css_property_compute_overflow_y;
        case LXB_CSS_PROPERTY_PADDING:
            return lxb_css_property_compute_padding;
        case LXB_CSS_PROPERTY_PADDING_BOTTOM:
            return lxb_css_property_compute_padding_bottom;
        case LXB_CSS_PROPERTY_PADDING_LEFT:
            return lxb_css_property_compute_padding_left;
        case LXB_CSS_PROPERTY_PADDING_RIGHT:
            return lxb_css_property_compute_padding_right;
        case LXB_CSS_PROPERTY_PADDING_TOP:
            return lxb_css_property_compute_padding_top;
        case LXB_CSS_PROPERTY_POSITION:
            return lxb_css_property_compute_position;
        case LXB_CSS_PROPERTY_RIGHT:
            return lxb_css_property_compute_right;
        case LXB_CSS_PROPERTY_TEXT_ALIGN:
            return lxb_css_property_compute_text_align;
        case LXB_CSS_PROPERTY_TOP:
            return lxb_css_property_compute_top;
        case LXB_CSS_PROPERTY_VISIBILITY:
            return lxb_css_property_compute_visibility;
        case LXB_CSS_PROPERTY_WHITE_SPACE:
            return lxb_css_property_compute_white_space;
        case LXB_CSS_PROPERTY_WIDTH:
            return lxb_css_property_compute_width;
        case LXB_CSS_PROPERTY_WRITING_MODE:
            return lxb_css_property_compute_writing_mode;
        case LXB_CSS_PROPERTY_Z_INDEX:
            return lxb_css_property_compute_z_index;
        default:
            return NULL;
    }
}

void
lxb_css_property_compute_display(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_display_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_DISPLAY);
    if (decl == NULL) {
        return;
    }

    p = decl->u.display;
    global = lxb_style_compute_global(p->a, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->display_a = c->parent->non_inherited->display_a;
        c->style->non_inherited->display_b = c->parent->non_inherited->display_b;
        c->style->non_inherited->display_c = c->parent->non_inherited->display_c;
        return;
    }

    if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->display_a = LXB_CSS_DISPLAY_INLINE;
        c->style->non_inherited->display_b = LXB_CSS_PROPERTY__UNDEF;
        c->style->non_inherited->display_c = LXB_CSS_PROPERTY__UNDEF;
        return;
    }

    c->style->non_inherited->display_a = p->a;
    c->style->non_inherited->display_b = p->b;
    c->style->non_inherited->display_c = p->c;
}

void
lxb_css_property_compute_position(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_position_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_POSITION);
    if (decl == NULL) {
        return;
    }

    p = decl->u.position;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->position = c->parent->non_inherited->position;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->position = LXB_CSS_POSITION_STATIC;
    }
    else {
        c->style->non_inherited->position = p->type;
    }
}

void
lxb_css_property_compute_box_sizing(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_box_sizing_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_BOX_SIZING);
    if (decl == NULL) {
        return;
    }

    p = decl->u.box_sizing;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->box_sizing = c->parent->non_inherited->box_sizing;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->box_sizing = LXB_CSS_BOX_SIZING_CONTENT_BOX;
    }
    else {
        c->style->non_inherited->box_sizing = p->type;
    }
}

void
lxb_css_property_compute_float(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_float_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_FLOAT);
    if (decl == NULL) {
        return;
    }

    p = decl->u.floatp;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->floatp = c->parent->non_inherited->floatp;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->floatp = LXB_CSS_FLOAT_NONE;
    }
    else {
        c->style->non_inherited->floatp = p->type;
    }
}

void
lxb_css_property_compute_clear(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_clear_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_CLEAR);
    if (decl == NULL) {
        return;
    }

    p = decl->u.clear;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->clear = c->parent->non_inherited->clear;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->clear = LXB_CSS_CLEAR_NONE;
    }
    else {
        c->style->non_inherited->clear = p->type;
    }
}

void
lxb_css_property_compute_direction(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_direction_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_DIRECTION);
    if (decl == NULL) {
        return;
    }

    p = decl->u.direction;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->direction = c->parent->inherited->direction;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->direction = LXB_CSS_DIRECTION_LTR;
    }
    else {
        c->style->inherited->direction = p->type;
    }
}

void
lxb_css_property_compute_writing_mode(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_writing_mode_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_WRITING_MODE);
    if (decl == NULL) {
        return;
    }

    p = decl->u.writing_mode;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->rare->writing_mode = c->parent->inherited->rare->writing_mode;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->rare->writing_mode = LXB_CSS_WRITING_MODE_HORIZONTAL_TB;
    }
    else {
        c->style->inherited->rare->writing_mode = p->type;
    }
}

void
lxb_css_property_compute_visibility(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_visibility_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_VISIBILITY);
    if (decl == NULL) {
        return;
    }

    p = decl->u.visibility;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->visibility = c->parent->inherited->visibility;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->visibility = LXB_CSS_VISIBILITY_VISIBLE;
    }
    else {
        c->style->inherited->visibility = p->type;
    }
}

void
lxb_css_property_compute_color(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_color_t *p;
    lxb_style_global_t global;
    lxb_style_color_t initial = lxb_style_color_rgba(0, 0, 0, 255);

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_COLOR);
    if (decl == NULL) {
        return;
    }

    p = decl->u.color;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->color = c->parent->inherited->color;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->color = initial;
    }
    else {
        c->style->inherited->color = lxb_style_compute_color_value(c, p, c->style->inherited->color,
                                                        initial);
    }
}

void
lxb_css_property_compute_background_color(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_background_color_t *p;
    lxb_style_global_t global;
    lxb_style_color_t initial = lxb_style_color_rgba(0, 0, 0, 0);

    decl = lxb_dom_element_style_by_id(c->element,
                                       LXB_CSS_PROPERTY_BACKGROUND_COLOR);
    if (decl == NULL) {
        return;
    }

    p = decl->u.background_color;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->visual->background_color = c->parent->visual->background_color;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->visual->background_color = initial;
    }
    else {
        c->style->visual->background_color = lxb_style_compute_color_value(c, p,
                                        c->style->inherited->color, initial);
    }
}

void
lxb_css_property_compute_font_size(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_font_size_t *p;
    lxb_style_global_t global;
    double parent_size;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_FONT_SIZE);
    if (decl == NULL) {
        return;
    }

    p = decl->u.font_size;
    global = lxb_style_compute_global(p->type, true, c->parent);
    parent_size = (c->parent != NULL) ? c->parent->inherited->font_size
                                      : lxb_style_compute_font_size_initial(c);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->font_size = parent_size;
        return;
    }

    if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->font_size = lxb_style_compute_font_size_initial(c);
        return;
    }

    switch (p->type) {
        case LXB_CSS_FONT_SIZE_XX_SMALL:
            c->style->inherited->font_size = 9.0;
            break;
        case LXB_CSS_FONT_SIZE_X_SMALL:
            c->style->inherited->font_size = 10.0;
            break;
        case LXB_CSS_FONT_SIZE_SMALL:
            c->style->inherited->font_size = 13.0;
            break;
        case LXB_CSS_FONT_SIZE_MEDIUM:
        case LXB_CSS_FONT_SIZE_MATH:
            c->style->inherited->font_size = 16.0;
            break;
        case LXB_CSS_FONT_SIZE_LARGE:
            c->style->inherited->font_size = 18.0;
            break;
        case LXB_CSS_FONT_SIZE_X_LARGE:
            c->style->inherited->font_size = 24.0;
            break;
        case LXB_CSS_FONT_SIZE_XX_LARGE:
            c->style->inherited->font_size = 32.0;
            break;
        case LXB_CSS_FONT_SIZE_XXX_LARGE:
            c->style->inherited->font_size = 48.0;
            break;
        case LXB_CSS_FONT_SIZE_LARGER:
            c->style->inherited->font_size = parent_size * 1.2;
            break;
        case LXB_CSS_FONT_SIZE_SMALLER:
            c->style->inherited->font_size = parent_size / 1.2;
            break;
        case LXB_CSS_FONT_SIZE__LENGTH:
            if (p->length.type == LXB_CSS_VALUE__PERCENTAGE) {
                c->style->inherited->font_size = parent_size
                                      * (p->length.u.percentage.num / 100.0);
            }
            else {
                c->style->inherited->font_size = lxb_style_compute_length_px(c,
                                                &p->length.u.length);
            }
            break;
        default:
            break;
    }
}

void
lxb_css_property_compute_font_weight(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_font_weight_t *p;
    lxb_style_global_t global;
    double parent_weight;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_FONT_WEIGHT);
    if (decl == NULL) {
        return;
    }

    p = decl->u.font_weight;
    global = lxb_style_compute_global(p->type, true, c->parent);
    parent_weight = (c->parent != NULL) ? c->parent->inherited->font_weight : 400.0;

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->font_weight_type = c->parent->inherited->font_weight_type;
        c->style->inherited->font_weight = c->parent->inherited->font_weight;
        return;
    }

    if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->font_weight_type = LXB_CSS_FONT_WEIGHT_NORMAL;
        c->style->inherited->font_weight = 400.0;
        return;
    }

    c->style->inherited->font_weight_type = p->type;

    switch (p->type) {
        case LXB_CSS_FONT_WEIGHT_NORMAL:
            c->style->inherited->font_weight = 400.0;
            break;
        case LXB_CSS_FONT_WEIGHT_BOLD:
            c->style->inherited->font_weight = 700.0;
            break;
        case LXB_CSS_FONT_WEIGHT_BOLDER:
            c->style->inherited->font_weight = (parent_weight < 600.0) ? 700.0 : 900.0;
            break;
        case LXB_CSS_FONT_WEIGHT_LIGHTER:
            c->style->inherited->font_weight = (parent_weight > 500.0) ? 400.0 : 100.0;
            break;
        case LXB_CSS_FONT_WEIGHT__NUMBER:
            c->style->inherited->font_weight = p->number.num;
            break;
        default:
            break;
    }
}

void
lxb_css_property_compute_font_style(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_font_style_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_FONT_STYLE);
    if (decl == NULL) {
        return;
    }

    p = decl->u.font_style;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->font_style = c->parent->inherited->font_style;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->font_style = LXB_CSS_FONT_STYLE_NORMAL;
    }
    else {
        c->style->inherited->font_style = p->type;
    }
}

void
lxb_css_property_compute_font_stretch(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_font_stretch_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_FONT_STRETCH);
    if (decl == NULL) {
        return;
    }

    p = decl->u.font_stretch;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->font_stretch_type = c->parent->inherited->font_stretch_type;
        c->style->inherited->font_stretch = c->parent->inherited->font_stretch;
        return;
    }

    if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->font_stretch_type = LXB_CSS_FONT_STRETCH_NORMAL;
        c->style->inherited->font_stretch = 100.0;
        return;
    }

    c->style->inherited->font_stretch_type = p->type;

    switch (p->type) {
        case LXB_CSS_FONT_STRETCH__PERCENTAGE:
            c->style->inherited->font_stretch = p->percentage.num;
            break;
        case LXB_CSS_FONT_STRETCH_ULTRA_CONDENSED:
            c->style->inherited->font_stretch = 50.0;
            break;
        case LXB_CSS_FONT_STRETCH_EXTRA_CONDENSED:
            c->style->inherited->font_stretch = 62.5;
            break;
        case LXB_CSS_FONT_STRETCH_CONDENSED:
            c->style->inherited->font_stretch = 75.0;
            break;
        case LXB_CSS_FONT_STRETCH_SEMI_CONDENSED:
            c->style->inherited->font_stretch = 87.5;
            break;
        case LXB_CSS_FONT_STRETCH_SEMI_EXPANDED:
            c->style->inherited->font_stretch = 112.5;
            break;
        case LXB_CSS_FONT_STRETCH_EXPANDED:
            c->style->inherited->font_stretch = 125.0;
            break;
        case LXB_CSS_FONT_STRETCH_EXTRA_EXPANDED:
            c->style->inherited->font_stretch = 150.0;
            break;
        case LXB_CSS_FONT_STRETCH_ULTRA_EXPANDED:
            c->style->inherited->font_stretch = 200.0;
            break;
        case LXB_CSS_FONT_STRETCH_NORMAL:
        default:
            c->style->inherited->font_stretch = 100.0;
            break;
    }
}

void
lxb_css_property_compute_line_height(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_line_height_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_LINE_HEIGHT);
    if (decl == NULL) {
        c->style->inherited->line_height = lxb_style_compute_line_height_initial(c,
                                                        c->style->inherited->font_size);
        return;
    }

    p = decl->u.line_height;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->line_height = c->parent->inherited->line_height;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL
             || p->type == LXB_CSS_LINE_HEIGHT_NORMAL)
    {
        c->style->inherited->line_height = lxb_style_compute_line_height_initial(c,
                                                        c->style->inherited->font_size);
    }
    else if (p->type == LXB_CSS_LINE_HEIGHT__NUMBER) {
        c->style->inherited->line_height = p->u.number.num * c->style->inherited->font_size;
    }
    else if (p->type == LXB_CSS_LINE_HEIGHT__PERCENTAGE) {
        c->style->inherited->line_height = p->u.percentage.num * c->style->inherited->font_size / 100.0;
    }
    else if (p->type == LXB_CSS_LINE_HEIGHT__LENGTH) {
        c->style->inherited->line_height = lxb_style_compute_length_px(c, &p->u.length);
    }
}

void
lxb_css_property_compute_width(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_WIDTH);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_WIDTH_AUTO);
    lxb_style_compute_edge_value(c, &c->style->box->width, decl->u.width,
                                 &c->parent->box->width,
                                 false, initial);
}

void
lxb_css_property_compute_height(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_HEIGHT);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_HEIGHT_AUTO);
    lxb_style_compute_edge_value(c, &c->style->box->height, decl->u.height,
                                 &c->parent->box->height,
                                 false, initial);
}

void
lxb_css_property_compute_min_width(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_MIN_WIDTH);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_MIN_WIDTH_AUTO);
    lxb_style_compute_edge_value(c, &c->style->box->min_width, decl->u.width,
                                 &c->parent->box->min_width, false, initial);
}

void
lxb_css_property_compute_min_height(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_MIN_HEIGHT);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_MIN_HEIGHT_AUTO);
    lxb_style_compute_edge_value(c, &c->style->box->min_height, decl->u.width,
                                 &c->parent->box->min_height, false, initial);
}

void
lxb_css_property_compute_max_width(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_MAX_WIDTH);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_MAX_WIDTH_NONE);
    lxb_style_compute_edge_value(c, &c->style->box->max_width, decl->u.width,
                                 &c->parent->box->max_width, false, initial);
}

void
lxb_css_property_compute_max_height(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_MAX_HEIGHT);
    if (decl == NULL) {
        return;
    }

    initial = lxb_style_compute_value_keyword(LXB_CSS_MAX_HEIGHT_NONE);
    lxb_style_compute_edge_value(c, &c->style->box->max_height, decl->u.width,
                                 &c->parent->box->max_height, false, initial);
}

#define LXB_STYLE_COMPUTE_INSET(name, prop_id, member, edge)                 \
void                                                                         \
lxb_css_property_compute_##name(void *ctx)                                   \
{                                                                            \
    lxb_style_compute_ctx_t *c = ctx;                                        \
    const lxb_css_rule_declaration_t *decl;                                  \
    lxb_style_computed_value_t initial;                                      \
                                                                             \
    decl = lxb_dom_element_style_by_id(c->element, prop_id);                 \
    if (decl == NULL) {                                                      \
        return;                                                              \
    }                                                                        \
                                                                             \
    initial = lxb_style_compute_value_keyword(LXB_CSS_VALUE_AUTO);           \
    lxb_style_compute_edge_value(c, &c->style->box->inset[edge],             \
                                 decl->u.member,                            \
                                 &c->parent->box->inset[edge],              \
                                 false, initial);                            \
}

LXB_STYLE_COMPUTE_INSET(top, LXB_CSS_PROPERTY_TOP, top, LXB_STYLE_EDGE_TOP)
LXB_STYLE_COMPUTE_INSET(right, LXB_CSS_PROPERTY_RIGHT, right, LXB_STYLE_EDGE_RIGHT)
LXB_STYLE_COMPUTE_INSET(bottom, LXB_CSS_PROPERTY_BOTTOM, bottom, LXB_STYLE_EDGE_BOTTOM)
LXB_STYLE_COMPUTE_INSET(left, LXB_CSS_PROPERTY_LEFT, left, LXB_STYLE_EDGE_LEFT)

#undef LXB_STYLE_COMPUTE_INSET

void
lxb_css_property_compute_margin(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_margin_t *p;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_MARGIN);
    if (decl == NULL) {
        return;
    }

    p = decl->u.margin;
    initial = lxb_style_compute_value_length(0.0);
    lxb_style_compute_edge_value(c, &c->style->surround->margin[LXB_STYLE_EDGE_TOP],
                                 &p->top,
                                 &c->parent->surround->margin[LXB_STYLE_EDGE_TOP],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->margin[LXB_STYLE_EDGE_RIGHT],
                                 &p->right,
                                 &c->parent->surround->margin[LXB_STYLE_EDGE_RIGHT],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->margin[LXB_STYLE_EDGE_BOTTOM],
                                 &p->bottom,
                                 &c->parent->surround->margin[LXB_STYLE_EDGE_BOTTOM],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->margin[LXB_STYLE_EDGE_LEFT],
                                 &p->left,
                                 &c->parent->surround->margin[LXB_STYLE_EDGE_LEFT],
                                 false, initial);
}

#define LXB_STYLE_COMPUTE_EDGE(name, prop_id, member, field, edge, init)      \
void                                                                         \
lxb_css_property_compute_##name(void *ctx)                                   \
{                                                                            \
    lxb_style_compute_ctx_t *c = ctx;                                        \
    const lxb_css_rule_declaration_t *decl;                                  \
                                                                             \
    decl = lxb_dom_element_style_by_id(c->element, prop_id);                 \
    if (decl == NULL) {                                                      \
        return;                                                              \
    }                                                                        \
                                                                             \
    lxb_style_compute_edge_value(c, &c->style->surround->field[edge],        \
                                 decl->u.member,                            \
                                 &c->parent->surround->field[edge],          \
                                 false, init);                               \
}

LXB_STYLE_COMPUTE_EDGE(margin_top, LXB_CSS_PROPERTY_MARGIN_TOP, margin_top,
                       margin, LXB_STYLE_EDGE_TOP,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(margin_right, LXB_CSS_PROPERTY_MARGIN_RIGHT, margin_right,
                       margin, LXB_STYLE_EDGE_RIGHT,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(margin_bottom, LXB_CSS_PROPERTY_MARGIN_BOTTOM, margin_bottom,
                       margin, LXB_STYLE_EDGE_BOTTOM,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(margin_left, LXB_CSS_PROPERTY_MARGIN_LEFT, margin_left,
                       margin, LXB_STYLE_EDGE_LEFT,
                       lxb_style_compute_value_length(0.0))

void
lxb_css_property_compute_padding(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_padding_t *p;
    lxb_style_computed_value_t initial;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_PADDING);
    if (decl == NULL) {
        return;
    }

    p = decl->u.padding;
    initial = lxb_style_compute_value_length(0.0);
    lxb_style_compute_edge_value(c, &c->style->surround->padding[LXB_STYLE_EDGE_TOP],
                                 &p->top,
                                 &c->parent->surround->padding[LXB_STYLE_EDGE_TOP],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->padding[LXB_STYLE_EDGE_RIGHT],
                                 &p->right,
                                 &c->parent->surround->padding[LXB_STYLE_EDGE_RIGHT],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->padding[LXB_STYLE_EDGE_BOTTOM],
                                 &p->bottom,
                                 &c->parent->surround->padding[LXB_STYLE_EDGE_BOTTOM],
                                 false, initial);
    lxb_style_compute_edge_value(c, &c->style->surround->padding[LXB_STYLE_EDGE_LEFT],
                                 &p->left,
                                 &c->parent->surround->padding[LXB_STYLE_EDGE_LEFT],
                                 false, initial);
}

LXB_STYLE_COMPUTE_EDGE(padding_top, LXB_CSS_PROPERTY_PADDING_TOP, padding_top,
                       padding, LXB_STYLE_EDGE_TOP,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(padding_right, LXB_CSS_PROPERTY_PADDING_RIGHT, padding_right,
                       padding, LXB_STYLE_EDGE_RIGHT,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(padding_bottom, LXB_CSS_PROPERTY_PADDING_BOTTOM, padding_bottom,
                       padding, LXB_STYLE_EDGE_BOTTOM,
                       lxb_style_compute_value_length(0.0))
LXB_STYLE_COMPUTE_EDGE(padding_left, LXB_CSS_PROPERTY_PADDING_LEFT, padding_left,
                       padding, LXB_STYLE_EDGE_LEFT,
                       lxb_style_compute_value_length(0.0))

#undef LXB_STYLE_COMPUTE_EDGE

void
lxb_css_property_compute_border(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    size_t i;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_BORDER);
    if (decl == NULL) {
        return;
    }

    for (i = 0; i < LXB_STYLE_EDGE__LAST_ENTRY; i++) {
        lxb_style_compute_border_value(c, &c->style->surround->border[i],
                                       decl->u.border,
                                       &c->parent->surround->border[i]);
    }
}

#define LXB_STYLE_COMPUTE_BORDER(name, prop_id, member, edge)                \
void                                                                         \
lxb_css_property_compute_##name(void *ctx)                                   \
{                                                                            \
    lxb_style_compute_ctx_t *c = ctx;                                        \
    const lxb_css_rule_declaration_t *decl;                                  \
                                                                             \
    decl = lxb_dom_element_style_by_id(c->element, prop_id);                 \
    if (decl == NULL) {                                                      \
        return;                                                              \
    }                                                                        \
                                                                             \
    lxb_style_compute_border_value(c, &c->style->surround->border[edge],     \
                                   decl->u.member,                          \
                                   &c->parent->surround->border[edge]);      \
}

LXB_STYLE_COMPUTE_BORDER(border_top, LXB_CSS_PROPERTY_BORDER_TOP, border_top,
                         LXB_STYLE_EDGE_TOP)
LXB_STYLE_COMPUTE_BORDER(border_right, LXB_CSS_PROPERTY_BORDER_RIGHT, border_right,
                         LXB_STYLE_EDGE_RIGHT)
LXB_STYLE_COMPUTE_BORDER(border_bottom, LXB_CSS_PROPERTY_BORDER_BOTTOM, border_bottom,
                         LXB_STYLE_EDGE_BOTTOM)
LXB_STYLE_COMPUTE_BORDER(border_left, LXB_CSS_PROPERTY_BORDER_LEFT, border_left,
                         LXB_STYLE_EDGE_LEFT)

#undef LXB_STYLE_COMPUTE_BORDER

#define LXB_STYLE_COMPUTE_BORDER_COLOR(name, prop_id, member, edge)          \
void                                                                         \
lxb_css_property_compute_##name(void *ctx)                                   \
{                                                                            \
    lxb_style_compute_ctx_t *c = ctx;                                        \
    const lxb_css_rule_declaration_t *decl;                                  \
    const lxb_css_value_color_t *p;                                          \
    lxb_style_global_t global;                                               \
    lxb_style_color_t initial;                                               \
                                                                             \
    decl = lxb_dom_element_style_by_id(c->element, prop_id);                 \
    if (decl == NULL) {                                                      \
        return;                                                              \
    }                                                                        \
                                                                             \
    p = decl->u.member;                                                      \
    initial = c->style->inherited->color;                                               \
    global = lxb_style_compute_global(p->type, false, c->parent);            \
                                                                             \
    if (global == LXB_STYLE_GLOBAL_PARENT) {                                 \
        c->style->surround->border[edge].color = c->parent->surround->border[edge].color;        \
    }                                                                        \
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {                           \
        c->style->surround->border[edge].color = initial;                              \
    }                                                                        \
    else {                                                                   \
        c->style->surround->border[edge].color = lxb_style_compute_color_value(c, p,   \
                                                c->style->inherited->color, initial);   \
    }                                                                        \
}

LXB_STYLE_COMPUTE_BORDER_COLOR(border_top_color,
                               LXB_CSS_PROPERTY_BORDER_TOP_COLOR,
                               border_top_color, LXB_STYLE_EDGE_TOP)
LXB_STYLE_COMPUTE_BORDER_COLOR(border_right_color,
                               LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR,
                               border_right_color, LXB_STYLE_EDGE_RIGHT)
LXB_STYLE_COMPUTE_BORDER_COLOR(border_bottom_color,
                               LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
                               border_bottom_color, LXB_STYLE_EDGE_BOTTOM)
LXB_STYLE_COMPUTE_BORDER_COLOR(border_left_color,
                               LXB_CSS_PROPERTY_BORDER_LEFT_COLOR,
                               border_left_color, LXB_STYLE_EDGE_LEFT)

#undef LXB_STYLE_COMPUTE_BORDER_COLOR

void
lxb_css_property_compute_overflow_x(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_overflow_x_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_OVERFLOW_X);
    if (decl == NULL) {
        return;
    }

    p = decl->u.overflow_x;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->overflow_x = c->parent->non_inherited->overflow_x;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->overflow_x = LXB_CSS_OVERFLOW_X_VISIBLE;
    }
    else {
        c->style->non_inherited->overflow_x = p->type;
    }
}

void
lxb_css_property_compute_overflow_y(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_overflow_y_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_OVERFLOW_Y);
    if (decl == NULL) {
        return;
    }

    p = decl->u.overflow_y;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->overflow_y = c->parent->non_inherited->overflow_y;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->overflow_y = LXB_CSS_OVERFLOW_Y_VISIBLE;
    }
    else {
        c->style->non_inherited->overflow_y = p->type;
    }
}

void
lxb_css_property_compute_opacity(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_opacity_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_OPACITY);
    if (decl == NULL) {
        return;
    }

    p = decl->u.opacity;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->opacity = c->parent->non_inherited->opacity;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->non_inherited->opacity = 1.0;
    }
    else {
        lxb_style_compute_number_percentage(p, &c->style->non_inherited->opacity);
    }

    if (c->style->non_inherited->opacity < 0.0) {
        c->style->non_inherited->opacity = 0.0;
    }
    else if (c->style->non_inherited->opacity > 1.0) {
        c->style->non_inherited->opacity = 1.0;
    }
}

void
lxb_css_property_compute_z_index(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_z_index_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_Z_INDEX);
    if (decl == NULL) {
        return;
    }

    p = decl->u.z_index;
    global = lxb_style_compute_global(p->type, false, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->non_inherited->z_index_auto = c->parent->non_inherited->z_index_auto;
        c->style->non_inherited->z_index = c->parent->non_inherited->z_index;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL || p->type == LXB_CSS_Z_INDEX_AUTO) {
        c->style->non_inherited->z_index_auto = true;
        c->style->non_inherited->z_index = 0;
    }
    else if (p->type == LXB_CSS_Z_INDEX__INTEGER) {
        c->style->non_inherited->z_index_auto = false;
        c->style->non_inherited->z_index = p->integer.num;
    }
}

void
lxb_css_property_compute_text_align(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_text_align_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_TEXT_ALIGN);
    if (decl == NULL) {
        return;
    }

    p = decl->u.text_align;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->rare->text_align = c->parent->inherited->rare->text_align;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->rare->text_align = LXB_CSS_TEXT_ALIGN_START;
    }
    else if (p->type == LXB_CSS_TEXT_ALIGN_MATCH_PARENT && c->parent != NULL) {
        c->style->inherited->rare->text_align = c->parent->inherited->rare->text_align;
    }
    else {
        c->style->inherited->rare->text_align = p->type;
    }
}

void
lxb_css_property_compute_white_space(void *ctx)
{
    lxb_style_compute_ctx_t *c = ctx;
    const lxb_css_rule_declaration_t *decl;
    const lxb_css_property_white_space_t *p;
    lxb_style_global_t global;

    decl = lxb_dom_element_style_by_id(c->element, LXB_CSS_PROPERTY_WHITE_SPACE);
    if (decl == NULL) {
        return;
    }

    p = decl->u.white_space;
    global = lxb_style_compute_global(p->type, true, c->parent);

    if (global == LXB_STYLE_GLOBAL_PARENT) {
        c->style->inherited->rare->white_space = c->parent->inherited->rare->white_space;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        c->style->inherited->rare->white_space = LXB_CSS_WHITE_SPACE_NORMAL;
    }
    else {
        c->style->inherited->rare->white_space = p->type;
    }
}


static lxb_style_global_t
lxb_style_compute_global(uintptr_t type, bool inherited,
                         const lxb_style_computed_t *parent)
{
    switch (type) {
        case LXB_CSS_VALUE_INHERIT:
            return (parent != NULL) ? LXB_STYLE_GLOBAL_PARENT
                                    : LXB_STYLE_GLOBAL_INITIAL;

        case LXB_CSS_VALUE_UNSET:
        case LXB_CSS_VALUE_REVERT:
            if (inherited && parent != NULL) {
                return LXB_STYLE_GLOBAL_PARENT;
            }

            return LXB_STYLE_GLOBAL_INITIAL;

        case LXB_CSS_VALUE_INITIAL:
            return LXB_STYLE_GLOBAL_INITIAL;

        default:
            return LXB_STYLE_GLOBAL_NONE;
    }
}

static double
lxb_style_compute_length_px(const lxb_style_compute_ctx_t *ctx,
                            const lxb_css_value_length_t *length)
{
    double font_size, line_height, viewport_width, viewport_height;

    font_size = ctx->style->inherited->font_size;
    line_height = ctx->style->inherited->line_height;
    viewport_width = ctx->ctx->viewport_width;
    viewport_height = ctx->ctx->viewport_height;

    switch (length->unit) {
        case LXB_CSS_UNIT_CM:
            return length->num * (96.0 / 2.54);
        case LXB_CSS_UNIT_MM:
            return length->num * (96.0 / 25.4);
        case LXB_CSS_UNIT_Q:
            return length->num * (96.0 / 101.6);
        case LXB_CSS_UNIT_IN:
            return length->num * 96.0;
        case LXB_CSS_UNIT_PC:
            return length->num * 16.0;
        case LXB_CSS_UNIT_PT:
            return length->num * (96.0 / 72.0);
        case LXB_CSS_UNIT_PX:
        case LXB_CSS_UNIT__UNDEF:
            return length->num;

        case LXB_CSS_UNIT_EM:
            return length->num * font_size;
        case LXB_CSS_UNIT_REM:
            return length->num * ctx->ctx->initial_font_size;
        case LXB_CSS_UNIT_EX:
        case LXB_CSS_UNIT_CH:
            return length->num * font_size * 0.5;
        case LXB_CSS_UNIT_CAP:
        case LXB_CSS_UNIT_IC:
            return length->num * font_size;
        case LXB_CSS_UNIT_LH:
            return length->num * line_height;
        case LXB_CSS_UNIT_RLH:
            return length->num * ctx->ctx->initial_line_height;
        case LXB_CSS_UNIT_VW:
        case LXB_CSS_UNIT_VI:
            return length->num * viewport_width / 100.0;
        case LXB_CSS_UNIT_VH:
        case LXB_CSS_UNIT_VB:
            return length->num * viewport_height / 100.0;
        case LXB_CSS_UNIT_VMIN:
            return length->num * lexbor_min(viewport_width, viewport_height) / 100.0;
        case LXB_CSS_UNIT_VMAX:
            return length->num * lexbor_max(viewport_width, viewport_height) / 100.0;

        default:
            return length->num;
    }
}

static void
lxb_style_compute_length_percentage(const lxb_style_compute_ctx_t *ctx,
                                    lxb_style_computed_value_t *dst,
                                    const lxb_css_value_length_percentage_t *src)
{
    switch (src->type) {
        case LXB_CSS_VALUE__LENGTH:
        case LXB_CSS_VALUE__NUMBER:
            *dst = lxb_style_compute_value_length(
                                    lxb_style_compute_length_px(ctx,
                                                                &src->u.length));
            break;
        case LXB_CSS_VALUE__PERCENTAGE:
            dst->type = LXB_STYLE_COMPUTED_VALUE_PERCENTAGE;
            dst->unit = LXB_CSS_UNIT__UNDEF;
            dst->num = src->u.percentage.num;
            dst->keyword = 0;
            break;
        default:
            *dst = lxb_style_compute_value_keyword(src->type);
            break;
    }
}

static void
lxb_style_compute_length_type(const lxb_style_compute_ctx_t *ctx,
                              lxb_style_computed_value_t *dst,
                              const lxb_css_value_length_type_t *src)
{
    if (src->type == LXB_CSS_VALUE__LENGTH) {
        *dst = lxb_style_compute_value_length(
                                    lxb_style_compute_length_px(ctx,
                                                                &src->length));
    }
    else {
        *dst = lxb_style_compute_value_keyword(src->type);
    }
}

static void
lxb_style_compute_number_percentage(const lxb_css_value_number_percentage_t *src,
                                    double *dst)
{
    if (src->type == LXB_CSS_VALUE__PERCENTAGE) {
        *dst = src->u.percentage.num / 100.0;
    }
    else {
        *dst = src->u.number.num;
    }
}

static void
lxb_style_compute_edge_value(const lxb_style_compute_ctx_t *ctx,
                             lxb_style_computed_value_t *dst,
                             const lxb_css_value_length_percentage_t *src,
                             const lxb_style_computed_value_t *parent,
                             bool inherited,
                             lxb_style_computed_value_t initial)
{
    lxb_style_global_t global;

    global = lxb_style_compute_global(src->type, inherited, ctx->parent);
    if (global == LXB_STYLE_GLOBAL_PARENT && parent != NULL) {
        *dst = *parent;
    }
    else if (global == LXB_STYLE_GLOBAL_INITIAL) {
        *dst = initial;
    }
    else {
        lxb_style_compute_length_percentage(ctx, dst, src);
    }
}

static void
lxb_style_compute_border_value(const lxb_style_compute_ctx_t *ctx,
                               lxb_style_computed_border_t *dst,
                               const lxb_css_property_border_t *src,
                               const lxb_style_computed_border_t *parent)
{
    lxb_style_global_t global;
    lxb_style_color_t initial_color;

    global = lxb_style_compute_global(src->style, false, ctx->parent);
    if (global == LXB_STYLE_GLOBAL_PARENT && parent != NULL) {
        *dst = *parent;
        return;
    }

    if (global == LXB_STYLE_GLOBAL_INITIAL) {
        dst->style = LXB_CSS_BORDER_NONE;
        dst->width = lxb_style_compute_value_length(0.0);
        dst->color = ctx->style->inherited->color;
        return;
    }

    dst->style = src->style;
    initial_color = ctx->style->inherited->color;
    dst->color = lxb_style_compute_color_value(ctx, &src->color,
                                               ctx->style->inherited->color,
                                               initial_color);

    switch (src->width.type) {
        case LXB_CSS_VALUE_THIN:
            dst->width = lxb_style_compute_value_length(1.0);
            break;
        case LXB_CSS_VALUE_THICK:
            dst->width = lxb_style_compute_value_length(5.0);
            break;
        case LXB_CSS_VALUE__LENGTH:
            lxb_style_compute_length_type(ctx, &dst->width, &src->width);
            break;
        case LXB_CSS_VALUE_MEDIUM:
        default:
            dst->width = lxb_style_compute_value_length(3.0);
            break;
    }

    if (dst->style == LXB_CSS_BORDER_NONE
        || dst->style == LXB_CSS_BORDER_HIDDEN)
    {
        dst->width = lxb_style_compute_value_length(0.0);
    }
}

static lxb_style_color_t
lxb_style_compute_color_value(const lxb_style_compute_ctx_t *ctx,
                              const lxb_css_value_color_t *src,
                              lxb_style_color_t current_color,
                              lxb_style_color_t initial)
{
    lxb_style_color_t color;
    double alpha;

    (void) ctx;

    switch (src->type) {
        case LXB_CSS_COLOR_CURRENTCOLOR:
            return current_color;

        case LXB_CSS_COLOR_TRANSPARENT:
            return lxb_style_color_rgba(0, 0, 0, 0);

        case LXB_CSS_COLOR_HEX:
            return lxb_style_color_rgba(src->u.hex.rgba.r,
                                        src->u.hex.rgba.g,
                                        src->u.hex.rgba.b,
                                        src->u.hex.rgba.a);

        case LXB_CSS_COLOR_RGB:
        case LXB_CSS_COLOR_RGBA:
            color = lxb_style_color_rgba(0, 0, 0, 255);

            if (src->u.rgb.r.type == LXB_CSS_VALUE__PERCENTAGE) {
                color.r = (uint8_t) (255.0 * src->u.rgb.r.u.percentage.num / 100.0);
                color.g = (uint8_t) (255.0 * src->u.rgb.g.u.percentage.num / 100.0);
                color.b = (uint8_t) (255.0 * src->u.rgb.b.u.percentage.num / 100.0);
            }
            else {
                color.r = (uint8_t) src->u.rgb.r.u.number.num;
                color.g = (uint8_t) src->u.rgb.g.u.number.num;
                color.b = (uint8_t) src->u.rgb.b.u.number.num;
            }

            if (src->u.rgb.a.type == LXB_CSS_VALUE_NONE) {
                color.a = 255;
            }
            else {
                lxb_style_compute_number_percentage(&src->u.rgb.a, &alpha);
                if (alpha < 0.0) {
                    alpha = 0.0;
                }
                else if (alpha > 1.0) {
                    alpha = 1.0;
                }

                color.a = (uint8_t) (alpha * 255.0);
            }

            return color;

        case LXB_CSS_COLOR_BLACK:
        case LXB_CSS_COLOR_CANVASTEXT:
            return lxb_style_color_rgba(0, 0, 0, 255);
        case LXB_CSS_COLOR_WHITE:
        case LXB_CSS_COLOR_CANVAS:
            return lxb_style_color_rgba(255, 255, 255, 255);
        case LXB_CSS_COLOR_RED:
            return lxb_style_color_rgba(255, 0, 0, 255);
        case LXB_CSS_COLOR_GREEN:
            return lxb_style_color_rgba(0, 128, 0, 255);
        case LXB_CSS_COLOR_BLUE:
            return lxb_style_color_rgba(0, 0, 255, 255);
        case LXB_CSS_COLOR_YELLOW:
            return lxb_style_color_rgba(255, 255, 0, 255);
        case LXB_CSS_COLOR_CYAN:
        case LXB_CSS_COLOR_AQUA:
            return lxb_style_color_rgba(0, 255, 255, 255);
        case LXB_CSS_COLOR_MAGENTA:
        case LXB_CSS_COLOR_FUCHSIA:
            return lxb_style_color_rgba(255, 0, 255, 255);
        case LXB_CSS_COLOR_GRAY:
        case LXB_CSS_COLOR_GREY:
            return lxb_style_color_rgba(128, 128, 128, 255);
        case LXB_CSS_COLOR_SILVER:
            return lxb_style_color_rgba(192, 192, 192, 255);
        case LXB_CSS_COLOR_MAROON:
            return lxb_style_color_rgba(128, 0, 0, 255);
        case LXB_CSS_COLOR_PURPLE:
            return lxb_style_color_rgba(128, 0, 128, 255);
        case LXB_CSS_COLOR_OLIVE:
            return lxb_style_color_rgba(128, 128, 0, 255);
        case LXB_CSS_COLOR_LIME:
            return lxb_style_color_rgba(0, 255, 0, 255);
        case LXB_CSS_COLOR_TEAL:
            return lxb_style_color_rgba(0, 128, 128, 255);
        case LXB_CSS_COLOR_NAVY:
            return lxb_style_color_rgba(0, 0, 128, 255);

        default:
            return initial;
    }
}

static lxb_style_computed_value_t
lxb_style_compute_value_keyword(uintptr_t keyword)
{
    lxb_style_computed_value_t value;

    value.type = (keyword == LXB_CSS_VALUE_AUTO)
                 ? LXB_STYLE_COMPUTED_VALUE_AUTO
                 : ((keyword == LXB_CSS_VALUE_NONE)
                    ? LXB_STYLE_COMPUTED_VALUE_NONE
                    : LXB_STYLE_COMPUTED_VALUE_KEYWORD);
    value.unit = LXB_CSS_UNIT__UNDEF;
    value.num = 0.0;
    value.keyword = keyword;

    return value;
}

static lxb_style_computed_value_t
lxb_style_compute_value_length(double px)
{
    lxb_style_computed_value_t value;

    value.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
    value.unit = LXB_CSS_UNIT_PX;
    value.num = px;
    value.keyword = 0;

    return value;
}

static double
lxb_style_compute_font_size_initial(const lxb_style_compute_ctx_t *ctx)
{
    return (ctx->ctx->initial_font_size > 0.0)
           ? ctx->ctx->initial_font_size : 16.0;
}

static double
lxb_style_compute_line_height_initial(const lxb_style_compute_ctx_t *ctx,
                                      double font_size)
{
    (void) ctx;

    return font_size * 1.2;
}

static lxb_style_color_t
lxb_style_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    lxb_style_color_t color;

    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return color;
}
