/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/style/computed.h"


static lxb_style_computed_rare_inherited_t *
lxb_style_computed_rare_inherited_create(void);

static lxb_style_computed_inherited_t *
lxb_style_computed_inherited_create(void);

static lxb_style_computed_non_inherited_t *
lxb_style_computed_non_inherited_create(void);

static lxb_style_computed_box_t *
lxb_style_computed_box_create(void);

static lxb_style_computed_surround_t *
lxb_style_computed_surround_create(void);

static lxb_style_computed_visual_t *
lxb_style_computed_visual_create(void);

static void
lxb_style_computed_rare_inherited_ref(lxb_style_computed_rare_inherited_t *data);

static void
lxb_style_computed_inherited_ref(lxb_style_computed_inherited_t *data);

static void
lxb_style_computed_non_inherited_ref(lxb_style_computed_non_inherited_t *data);

static void
lxb_style_computed_box_ref(lxb_style_computed_box_t *data);

static void
lxb_style_computed_surround_ref(lxb_style_computed_surround_t *data);

static void
lxb_style_computed_visual_ref(lxb_style_computed_visual_t *data);

static void
lxb_style_computed_rare_inherited_unref(lxb_style_computed_rare_inherited_t *data);

static void
lxb_style_computed_inherited_unref(lxb_style_computed_inherited_t *data);

static void
lxb_style_computed_non_inherited_unref(lxb_style_computed_non_inherited_t *data);

static void
lxb_style_computed_box_unref(lxb_style_computed_box_t *data);

static void
lxb_style_computed_surround_unref(lxb_style_computed_surround_t *data);

static void
lxb_style_computed_visual_unref(lxb_style_computed_visual_t *data);

static bool
lxb_style_computed_rare_inherited_equal(
                    const lxb_style_computed_rare_inherited_t *left,
                    const lxb_style_computed_rare_inherited_t *right);

static bool
lxb_style_computed_inherited_equal(
                    const lxb_style_computed_inherited_t *left,
                    const lxb_style_computed_inherited_t *right);

static bool
lxb_style_computed_non_inherited_equal(
                    const lxb_style_computed_non_inherited_t *left,
                    const lxb_style_computed_non_inherited_t *right);

static bool
lxb_style_computed_box_equal(const lxb_style_computed_box_t *left,
                             const lxb_style_computed_box_t *right);

static bool
lxb_style_computed_surround_equal(const lxb_style_computed_surround_t *left,
                                  const lxb_style_computed_surround_t *right);

static bool
lxb_style_computed_visual_equal(const lxb_style_computed_visual_t *left,
                                const lxb_style_computed_visual_t *right);

static lxb_status_t
lxb_style_computed_detach_rare_inherited(
                    lxb_style_computed_inherited_t *inherited);


lxb_style_computed_t *
lxb_style_computed_create(void)
{
    lxb_style_computed_t *style;

    style = lexbor_calloc(1, sizeof(lxb_style_computed_t));
    if (style != NULL) {
        style->refs = 1;
    }

    return style;
}

lxb_style_computed_t *
lxb_style_computed_create_initial(const lxb_style_computed_t *initial,
                                  const lxb_style_computed_t *parent)
{
    lxb_style_computed_t *style;

    if (initial == NULL) {
        return NULL;
    }

    style = lxb_style_computed_create();
    if (style == NULL) {
        return NULL;
    }

    style->inherited = (parent != NULL) ? parent->inherited : initial->inherited;
    style->non_inherited = initial->non_inherited;
    style->box = initial->box;
    style->surround = initial->surround;
    style->visual = initial->visual;

    lxb_style_computed_inherited_ref(style->inherited);
    lxb_style_computed_non_inherited_ref(style->non_inherited);
    lxb_style_computed_box_ref(style->box);
    lxb_style_computed_surround_ref(style->surround);
    lxb_style_computed_visual_ref(style->visual);

    return style;
}

void
lxb_style_computed_destroy(lxb_style_computed_t *style)
{
    if (style == NULL) {
        return;
    }

    lxb_style_computed_inherited_unref(style->inherited);
    lxb_style_computed_non_inherited_unref(style->non_inherited);
    lxb_style_computed_box_unref(style->box);
    lxb_style_computed_surround_unref(style->surround);
    lxb_style_computed_visual_unref(style->visual);

    style->inherited = NULL;
    style->non_inherited = NULL;
    style->box = NULL;
    style->surround = NULL;
    style->visual = NULL;
}

void
lxb_style_computed_ref(lxb_style_computed_t *style)
{
    if (style != NULL && style->refs != SIZE_MAX) {
        style->refs++;
    }
}

void
lxb_style_computed_unref(lxb_style_computed_t *style)
{
    if (style == NULL) {
        return;
    }

    if (style->refs == SIZE_MAX) {
        return;
    }

    style->refs--;

    if (style->refs == 0) {
        lxb_style_computed_destroy(style);
        lexbor_free(style);
    }
}

bool
lxb_style_computed_equal(const lxb_style_computed_t *left,
                         const lxb_style_computed_t *right)
{
    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    return lxb_style_computed_inherited_equal(left->inherited, right->inherited)
        && lxb_style_computed_non_inherited_equal(left->non_inherited,
                                                 right->non_inherited)
        && lxb_style_computed_box_equal(left->box, right->box)
        && lxb_style_computed_surround_equal(left->surround, right->surround)
        && lxb_style_computed_visual_equal(left->visual, right->visual);
}

lxb_status_t
lxb_style_computed_set_initial(lxb_style_computed_t *style,
                               double initial_font_size,
                               double initial_line_height)
{
    size_t i;
    lxb_style_computed_inherited_t *inherited;
    lxb_style_computed_rare_inherited_t *rare;
    lxb_style_computed_non_inherited_t *non_inherited;
    lxb_style_computed_box_t *box;
    lxb_style_computed_surround_t *surround;
    lxb_style_computed_visual_t *visual;

    if (style == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    lxb_style_computed_destroy(style);
    memset(style, 0, sizeof(lxb_style_computed_t));
    style->refs = 1;

    inherited = lxb_style_computed_inherited_create();
    rare = lxb_style_computed_rare_inherited_create();
    non_inherited = lxb_style_computed_non_inherited_create();
    box = lxb_style_computed_box_create();
    surround = lxb_style_computed_surround_create();
    visual = lxb_style_computed_visual_create();

    if (inherited == NULL || rare == NULL || non_inherited == NULL
        || box == NULL || surround == NULL || visual == NULL)
    {
        lxb_style_computed_inherited_unref(inherited);
        lxb_style_computed_rare_inherited_unref(rare);
        lxb_style_computed_non_inherited_unref(non_inherited);
        lxb_style_computed_box_unref(box);
        lxb_style_computed_surround_unref(surround);
        lxb_style_computed_visual_unref(visual);
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    inherited->rare = rare;
    rare = NULL;

    inherited->color.r = 0;
    inherited->color.g = 0;
    inherited->color.b = 0;
    inherited->color.a = 255;
    inherited->direction = LXB_CSS_DIRECTION_LTR;
    inherited->visibility = LXB_CSS_VISIBILITY_VISIBLE;
    inherited->font_size = initial_font_size;
    inherited->line_height = initial_line_height;
    inherited->font_weight_type = LXB_CSS_FONT_WEIGHT_NORMAL;
    inherited->font_weight = 400.0;
    inherited->font_style = LXB_CSS_FONT_STYLE_NORMAL;
    inherited->font_stretch_type = LXB_CSS_FONT_STRETCH_NORMAL;
    inherited->font_stretch = 100.0;

    inherited->rare->text_align = LXB_CSS_TEXT_ALIGN_START;
    inherited->rare->white_space = LXB_CSS_WHITE_SPACE_NORMAL;
    inherited->rare->writing_mode = LXB_CSS_WRITING_MODE_HORIZONTAL_TB;

    non_inherited->display_a = LXB_CSS_DISPLAY_INLINE;
    non_inherited->display_b = LXB_CSS_PROPERTY__UNDEF;
    non_inherited->display_c = LXB_CSS_PROPERTY__UNDEF;
    non_inherited->position = LXB_CSS_POSITION_STATIC;
    non_inherited->box_sizing = LXB_CSS_BOX_SIZING_CONTENT_BOX;
    non_inherited->floatp = LXB_CSS_FLOAT_NONE;
    non_inherited->clear = LXB_CSS_CLEAR_NONE;
    non_inherited->overflow_x = LXB_CSS_OVERFLOW_X_VISIBLE;
    non_inherited->overflow_y = LXB_CSS_OVERFLOW_Y_VISIBLE;
    non_inherited->opacity = 1.0;
    non_inherited->z_index_auto = true;
    non_inherited->z_index = 0;

    visual->background_color.r = 0;
    visual->background_color.g = 0;
    visual->background_color.b = 0;
    visual->background_color.a = 0;

    box->width.type = LXB_STYLE_COMPUTED_VALUE_AUTO;
    box->width.keyword = LXB_CSS_WIDTH_AUTO;
    box->height.type = LXB_STYLE_COMPUTED_VALUE_AUTO;
    box->height.keyword = LXB_CSS_HEIGHT_AUTO;
    box->min_width.type = LXB_STYLE_COMPUTED_VALUE_AUTO;
    box->min_width.keyword = LXB_CSS_MIN_WIDTH_AUTO;
    box->min_height.type = LXB_STYLE_COMPUTED_VALUE_AUTO;
    box->min_height.keyword = LXB_CSS_MIN_HEIGHT_AUTO;
    box->max_width.type = LXB_STYLE_COMPUTED_VALUE_NONE;
    box->max_width.keyword = LXB_CSS_MAX_WIDTH_NONE;
    box->max_height.type = LXB_STYLE_COMPUTED_VALUE_NONE;
    box->max_height.keyword = LXB_CSS_MAX_HEIGHT_NONE;

    for (i = 0; i < LXB_STYLE_EDGE__LAST_ENTRY; i++) {
        box->inset[i].type = LXB_STYLE_COMPUTED_VALUE_AUTO;
        box->inset[i].keyword = LXB_CSS_VALUE_AUTO;

        surround->margin[i].type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        surround->margin[i].unit = LXB_CSS_UNIT_PX;

        surround->padding[i].type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        surround->padding[i].unit = LXB_CSS_UNIT_PX;

        surround->border[i].width.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        surround->border[i].width.unit = LXB_CSS_UNIT_PX;
        surround->border[i].style = LXB_CSS_BORDER_NONE;
        surround->border[i].color = inherited->color;
    }

    style->inherited = inherited;
    style->non_inherited = non_inherited;
    style->box = box;
    style->surround = surround;
    style->visual = visual;

    return LXB_STATUS_OK;
}

lxb_style_computed_inherited_t *
lxb_style_computed_inherited_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_inherited_t *copy;

    if (style == NULL || style->inherited == NULL) {
        return NULL;
    }

    if (style->inherited->refs == 1) {
        return style->inherited;
    }

    copy = lxb_style_computed_inherited_create();
    if (copy == NULL) {
        return NULL;
    }

    *copy = *style->inherited;
    copy->refs = 1;
    lxb_style_computed_rare_inherited_ref(copy->rare);
    lxb_style_computed_inherited_unref(style->inherited);
    style->inherited = copy;

    return copy;
}

lxb_style_computed_rare_inherited_t *
lxb_style_computed_rare_inherited_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_inherited_t *inherited;

    inherited = lxb_style_computed_inherited_mutable(style);
    if (inherited == NULL) {
        return NULL;
    }

    if (lxb_style_computed_detach_rare_inherited(inherited) != LXB_STATUS_OK) {
        return NULL;
    }

    return inherited->rare;
}

lxb_style_computed_non_inherited_t *
lxb_style_computed_non_inherited_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_non_inherited_t *copy;

    if (style == NULL || style->non_inherited == NULL) {
        return NULL;
    }

    if (style->non_inherited->refs == 1) {
        return style->non_inherited;
    }

    copy = lxb_style_computed_non_inherited_create();
    if (copy == NULL) {
        return NULL;
    }

    *copy = *style->non_inherited;
    copy->refs = 1;
    lxb_style_computed_non_inherited_unref(style->non_inherited);
    style->non_inherited = copy;

    return copy;
}

lxb_style_computed_box_t *
lxb_style_computed_box_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_box_t *copy;

    if (style == NULL || style->box == NULL) {
        return NULL;
    }

    if (style->box->refs == 1) {
        return style->box;
    }

    copy = lxb_style_computed_box_create();
    if (copy == NULL) {
        return NULL;
    }

    *copy = *style->box;
    copy->refs = 1;
    lxb_style_computed_box_unref(style->box);
    style->box = copy;

    return copy;
}

lxb_style_computed_surround_t *
lxb_style_computed_surround_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_surround_t *copy;

    if (style == NULL || style->surround == NULL) {
        return NULL;
    }

    if (style->surround->refs == 1) {
        return style->surround;
    }

    copy = lxb_style_computed_surround_create();
    if (copy == NULL) {
        return NULL;
    }

    *copy = *style->surround;
    copy->refs = 1;
    lxb_style_computed_surround_unref(style->surround);
    style->surround = copy;

    return copy;
}

lxb_style_computed_visual_t *
lxb_style_computed_visual_mutable(lxb_style_computed_t *style)
{
    lxb_style_computed_visual_t *copy;

    if (style == NULL || style->visual == NULL) {
        return NULL;
    }

    if (style->visual->refs == 1) {
        return style->visual;
    }

    copy = lxb_style_computed_visual_create();
    if (copy == NULL) {
        return NULL;
    }

    *copy = *style->visual;
    copy->refs = 1;
    lxb_style_computed_visual_unref(style->visual);
    style->visual = copy;

    return copy;
}

lxb_status_t
lxb_style_computed_detach_for_property(lxb_style_computed_t *style,
                                       uintptr_t id)
{
    switch (id) {
        case LXB_CSS_PROPERTY_COLOR:
        case LXB_CSS_PROPERTY_DIRECTION:
        case LXB_CSS_PROPERTY_FONT_SIZE:
        case LXB_CSS_PROPERTY_FONT_STRETCH:
        case LXB_CSS_PROPERTY_FONT_STYLE:
        case LXB_CSS_PROPERTY_FONT_WEIGHT:
        case LXB_CSS_PROPERTY_LINE_HEIGHT:
        case LXB_CSS_PROPERTY_VISIBILITY:
            return (lxb_style_computed_inherited_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        case LXB_CSS_PROPERTY_TEXT_ALIGN:
        case LXB_CSS_PROPERTY_WHITE_SPACE:
        case LXB_CSS_PROPERTY_WRITING_MODE:
            return (lxb_style_computed_rare_inherited_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        case LXB_CSS_PROPERTY_DISPLAY:
        case LXB_CSS_PROPERTY_POSITION:
        case LXB_CSS_PROPERTY_BOX_SIZING:
        case LXB_CSS_PROPERTY_FLOAT:
        case LXB_CSS_PROPERTY_CLEAR:
        case LXB_CSS_PROPERTY_OVERFLOW_X:
        case LXB_CSS_PROPERTY_OVERFLOW_Y:
        case LXB_CSS_PROPERTY_OPACITY:
        case LXB_CSS_PROPERTY_Z_INDEX:
            return (lxb_style_computed_non_inherited_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        case LXB_CSS_PROPERTY_WIDTH:
        case LXB_CSS_PROPERTY_HEIGHT:
        case LXB_CSS_PROPERTY_MIN_WIDTH:
        case LXB_CSS_PROPERTY_MIN_HEIGHT:
        case LXB_CSS_PROPERTY_MAX_WIDTH:
        case LXB_CSS_PROPERTY_MAX_HEIGHT:
        case LXB_CSS_PROPERTY_TOP:
        case LXB_CSS_PROPERTY_RIGHT:
        case LXB_CSS_PROPERTY_BOTTOM:
        case LXB_CSS_PROPERTY_LEFT:
            return (lxb_style_computed_box_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        case LXB_CSS_PROPERTY_MARGIN:
        case LXB_CSS_PROPERTY_MARGIN_TOP:
        case LXB_CSS_PROPERTY_MARGIN_RIGHT:
        case LXB_CSS_PROPERTY_MARGIN_BOTTOM:
        case LXB_CSS_PROPERTY_MARGIN_LEFT:
        case LXB_CSS_PROPERTY_PADDING:
        case LXB_CSS_PROPERTY_PADDING_TOP:
        case LXB_CSS_PROPERTY_PADDING_RIGHT:
        case LXB_CSS_PROPERTY_PADDING_BOTTOM:
        case LXB_CSS_PROPERTY_PADDING_LEFT:
        case LXB_CSS_PROPERTY_BORDER:
        case LXB_CSS_PROPERTY_BORDER_TOP:
        case LXB_CSS_PROPERTY_BORDER_RIGHT:
        case LXB_CSS_PROPERTY_BORDER_BOTTOM:
        case LXB_CSS_PROPERTY_BORDER_LEFT:
        case LXB_CSS_PROPERTY_BORDER_TOP_COLOR:
        case LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR:
        case LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
        case LXB_CSS_PROPERTY_BORDER_LEFT_COLOR:
            return (lxb_style_computed_surround_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        case LXB_CSS_PROPERTY_BACKGROUND_COLOR:
            return (lxb_style_computed_visual_mutable(style) != NULL)
                   ? LXB_STATUS_OK : LXB_STATUS_ERROR_MEMORY_ALLOCATION;

        default:
            return LXB_STATUS_OK;
    }
}

static lxb_style_computed_rare_inherited_t *
lxb_style_computed_rare_inherited_create(void)
{
    lxb_style_computed_rare_inherited_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_rare_inherited_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static lxb_style_computed_inherited_t *
lxb_style_computed_inherited_create(void)
{
    lxb_style_computed_inherited_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_inherited_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static lxb_style_computed_non_inherited_t *
lxb_style_computed_non_inherited_create(void)
{
    lxb_style_computed_non_inherited_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_non_inherited_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static lxb_style_computed_box_t *
lxb_style_computed_box_create(void)
{
    lxb_style_computed_box_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_box_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static lxb_style_computed_surround_t *
lxb_style_computed_surround_create(void)
{
    lxb_style_computed_surround_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_surround_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static lxb_style_computed_visual_t *
lxb_style_computed_visual_create(void)
{
    lxb_style_computed_visual_t *data;

    data = lexbor_calloc(1, sizeof(lxb_style_computed_visual_t));
    if (data != NULL) {
        data->refs = 1;
    }

    return data;
}

static void
lxb_style_computed_rare_inherited_ref(lxb_style_computed_rare_inherited_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_inherited_ref(lxb_style_computed_inherited_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_non_inherited_ref(lxb_style_computed_non_inherited_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_box_ref(lxb_style_computed_box_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_surround_ref(lxb_style_computed_surround_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_visual_ref(lxb_style_computed_visual_t *data)
{
    if (data != NULL) {
        data->refs++;
    }
}

static void
lxb_style_computed_rare_inherited_unref(lxb_style_computed_rare_inherited_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lexbor_free(data);
    }
}

static void
lxb_style_computed_inherited_unref(lxb_style_computed_inherited_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lxb_style_computed_rare_inherited_unref(data->rare);
        lexbor_free(data);
    }
}

static void
lxb_style_computed_non_inherited_unref(lxb_style_computed_non_inherited_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lexbor_free(data);
    }
}

static void
lxb_style_computed_box_unref(lxb_style_computed_box_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lexbor_free(data);
    }
}

static void
lxb_style_computed_surround_unref(lxb_style_computed_surround_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lexbor_free(data);
    }
}

static void
lxb_style_computed_visual_unref(lxb_style_computed_visual_t *data)
{
    if (data == NULL) {
        return;
    }

    data->refs--;

    if (data->refs == 0) {
        lexbor_free(data);
    }
}

static bool
lxb_style_computed_rare_inherited_equal(
                    const lxb_style_computed_rare_inherited_t *left,
                    const lxb_style_computed_rare_inherited_t *right)
{
    lxb_style_computed_rare_inherited_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;

    return memcmp(&l, &r, sizeof(lxb_style_computed_rare_inherited_t)) == 0;
}

static bool
lxb_style_computed_inherited_equal(
                    const lxb_style_computed_inherited_t *left,
                    const lxb_style_computed_inherited_t *right)
{
    lxb_style_computed_inherited_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;
    l.rare = NULL;
    r.rare = NULL;

    return memcmp(&l, &r, sizeof(lxb_style_computed_inherited_t)) == 0
        && lxb_style_computed_rare_inherited_equal(left->rare, right->rare);
}

static bool
lxb_style_computed_non_inherited_equal(
                    const lxb_style_computed_non_inherited_t *left,
                    const lxb_style_computed_non_inherited_t *right)
{
    lxb_style_computed_non_inherited_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;

    return memcmp(&l, &r, sizeof(lxb_style_computed_non_inherited_t)) == 0;
}

static bool
lxb_style_computed_box_equal(const lxb_style_computed_box_t *left,
                             const lxb_style_computed_box_t *right)
{
    lxb_style_computed_box_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;

    return memcmp(&l, &r, sizeof(lxb_style_computed_box_t)) == 0;
}

static bool
lxb_style_computed_surround_equal(const lxb_style_computed_surround_t *left,
                                  const lxb_style_computed_surround_t *right)
{
    lxb_style_computed_surround_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;

    return memcmp(&l, &r, sizeof(lxb_style_computed_surround_t)) == 0;
}

static bool
lxb_style_computed_visual_equal(const lxb_style_computed_visual_t *left,
                                const lxb_style_computed_visual_t *right)
{
    lxb_style_computed_visual_t l, r;

    if (left == right) {
        return true;
    }

    if (left == NULL || right == NULL) {
        return false;
    }

    l = *left;
    r = *right;
    l.refs = 0;
    r.refs = 0;

    return memcmp(&l, &r, sizeof(lxb_style_computed_visual_t)) == 0;
}

static lxb_status_t
lxb_style_computed_detach_rare_inherited(
                    lxb_style_computed_inherited_t *inherited)
{
    lxb_style_computed_rare_inherited_t *copy;

    if (inherited == NULL || inherited->rare == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    if (inherited->rare->refs == 1) {
        return LXB_STATUS_OK;
    }

    copy = lxb_style_computed_rare_inherited_create();
    if (copy == NULL) {
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    *copy = *inherited->rare;
    copy->refs = 1;
    lxb_style_computed_rare_inherited_unref(inherited->rare);
    inherited->rare = copy;

    return LXB_STATUS_OK;
}
