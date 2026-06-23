#include "layout/layout_internal.h"
#include "layout/layout_algorithm.h"

#include <string.h>

#include "lexbor/css/property/const.h"

typedef struct {
    double top;
    double right;
    double bottom;
    double left;
} layout_reflow_edges_t;

typedef struct {
    bool top;
    bool right;
    bool bottom;
    bool left;
} layout_reflow_edge_auto_t;

typedef struct layout_reflow_ancestor {
    layout_object_t *object;
    layout_fragment_t *fragment;
    layout_point_t origin;
    layout_size_t size;
    layout_point_t padding_box_offset;
    layout_size_t padding_box_size;
    layout_point_t content_offset;
    layout_size_t content_size;
    const struct layout_reflow_ancestor *parent;
} layout_reflow_ancestor_t;

typedef struct {
    layout_tree_t *tree;
    layout_result_t *result;
    const layout_fragment_builder_t *builder;
    double default_block_gap;
} layout_reflow_context_t;

typedef struct {
    layout_object_t *object;
    layout_size_t containing_size;
    layout_point_t origin;
    const layout_reflow_ancestor_t *ancestor;
    bool has_parent;
} layout_reflow_input_t;

typedef enum {
    LAYOUT_REFLOW_FLOAT_NONE = 0,
    LAYOUT_REFLOW_FLOAT_LEFT = 1 << 0,
    LAYOUT_REFLOW_FLOAT_RIGHT = 1 << 1,
    LAYOUT_REFLOW_FLOAT_BOTH = LAYOUT_REFLOW_FLOAT_LEFT
        | LAYOUT_REFLOW_FLOAT_RIGHT
} layout_reflow_float_side_t;

#define LAYOUT_REFLOW_MAX_FLOATS 32

typedef struct {
    layout_reflow_float_side_t side;
    double top;
    double bottom;
    double width;
} layout_reflow_float_entry_t;

typedef struct {
    layout_reflow_float_entry_t entries[LAYOUT_REFLOW_MAX_FLOATS];
    size_t count;
} layout_reflow_float_state_t;

typedef struct {
    layout_reflow_edges_t raw_margin;
    layout_reflow_edges_t margin;
    layout_reflow_edge_auto_t margin_auto;
    layout_reflow_edges_t padding;
    layout_reflow_edges_t border;
    layout_size_t border_size;
    layout_size_t content_size;
    bool explicit_width;
    bool explicit_height;
    bool has_adjoining_margin_top;
    bool has_adjoining_margin_bottom;
    double adjoining_margin_top;
    double adjoining_margin_bottom;
    bool collapses_own_margins;
    double collapsed_margin;
    layout_reflow_float_state_t escaped_floats;
} layout_reflow_measure_t;

typedef struct {
    bool has_top;
    bool has_bottom;
    double top;
    double bottom;
} layout_reflow_collapsed_child_margins_t;

typedef struct {
    double top;
    double right;
    double bottom;
    double left;
    bool has_explicit;
    bool top_auto;
    bool right_auto;
    bool bottom_auto;
    bool left_auto;
} layout_reflow_insets_t;

static const layout_transform_t layout_algorithm_identity_transform =
    {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

static bool
layout_reflow_length_percentage(const lxb_style_computed_value_t *value,
                                double percent_basis,
                                double *out_value)
{
    if (value == NULL || out_value == NULL) {
        return false;
    }

    if (value->type == LXB_STYLE_COMPUTED_VALUE_LENGTH
        && value->u.length.unit == (lxb_css_unit_t) LXB_CSS_UNIT_PX) {
        *out_value = value->u.length.num;
        return true;
    }

    if (value->type == LXB_STYLE_COMPUTED_VALUE_PERCENTAGE) {
        *out_value = percent_basis * value->u.number / 100.0;
        return true;
    }

    return false;
}

static double
layout_reflow_non_negative(double value)
{
    return value < 0.0 ? 0.0 : value;
}

static double
layout_reflow_edge_value(const lxb_style_computed_value_t *value,
                         double percent_basis)
{
    double resolved = 0.0;

    return layout_reflow_length_percentage(value, percent_basis, &resolved)
        ? resolved : 0.0;
}

static bool
layout_reflow_edge_is_auto(const lxb_style_computed_value_t *value)
{
    return value != NULL && value->type == LXB_STYLE_COMPUTED_VALUE_AUTO;
}

static bool
layout_reflow_inset_value(const lxb_style_computed_value_t *value,
                          double percent_basis,
                          double *out_value)
{
    if (out_value != NULL) {
        *out_value = 0.0;
    }

    if (value == NULL || out_value == NULL
        || value->type == LXB_STYLE_COMPUTED_VALUE_AUTO
        || value->type == LXB_STYLE_COMPUTED_VALUE_NONE) {
        return false;
    }

    return layout_reflow_length_percentage(value, percent_basis, out_value);
}

static layout_reflow_insets_t
layout_reflow_insets_from_style(const lxb_style_computed_t *style,
                                layout_size_t containing_size)
{
    layout_reflow_insets_t insets;

    memset(&insets, 0, sizeof(insets));
    insets.top_auto = true;
    insets.right_auto = true;
    insets.bottom_auto = true;
    insets.left_auto = true;

    if (style == NULL || style->box == NULL) {
        return insets;
    }

    insets.top_auto = !layout_reflow_inset_value(
        &style->box->inset[LXB_STYLE_EDGE_TOP], containing_size.height,
        &insets.top);
    insets.right_auto = !layout_reflow_inset_value(
        &style->box->inset[LXB_STYLE_EDGE_RIGHT], containing_size.width,
        &insets.right);
    insets.bottom_auto = !layout_reflow_inset_value(
        &style->box->inset[LXB_STYLE_EDGE_BOTTOM], containing_size.height,
        &insets.bottom);
    insets.left_auto = !layout_reflow_inset_value(
        &style->box->inset[LXB_STYLE_EDGE_LEFT], containing_size.width,
        &insets.left);
    insets.has_explicit = !insets.top_auto || !insets.right_auto
        || !insets.bottom_auto || !insets.left_auto;

    return insets;
}

static bool
layout_reflow_object_has_positioned_inset(layout_object_t *object,
                                          layout_size_t containing_size)
{
    layout_reflow_insets_t insets;

    if (object == NULL) {
        return false;
    }

    insets = layout_reflow_insets_from_style(layout_object_style(object),
                                             containing_size);
    return insets.has_explicit;
}

static double
layout_reflow_border_width(const lxb_style_computed_border_t *border)
{
    if (border == NULL || border->style == LXB_CSS_BORDER_NONE
        || border->style == LXB_CSS_BORDER_HIDDEN) {
        return 0.0;
    }

    return layout_reflow_edge_value(&border->width, 0.0);
}

static void
layout_reflow_edges_from_style(const lxb_style_computed_t *style,
                               double percent_basis,
                               layout_reflow_edges_t *margin,
                               layout_reflow_edge_auto_t *margin_auto,
                               layout_reflow_edges_t *padding,
                               layout_reflow_edges_t *border)
{
    const lxb_style_computed_surround_t *surround;

    if (margin != NULL) {
        memset(margin, 0, sizeof(*margin));
    }

    if (margin_auto != NULL) {
        memset(margin_auto, 0, sizeof(*margin_auto));
    }

    if (padding != NULL) {
        memset(padding, 0, sizeof(*padding));
    }

    if (border != NULL) {
        memset(border, 0, sizeof(*border));
    }

    if (style == NULL || style->surround == NULL) {
        return;
    }

    surround = style->surround;

    if (margin_auto != NULL) {
        margin_auto->top =
            layout_reflow_edge_is_auto(&surround->margin[LXB_STYLE_EDGE_TOP]);
        margin_auto->right =
            layout_reflow_edge_is_auto(&surround->margin[LXB_STYLE_EDGE_RIGHT]);
        margin_auto->bottom = layout_reflow_edge_is_auto(
            &surround->margin[LXB_STYLE_EDGE_BOTTOM]);
        margin_auto->left =
            layout_reflow_edge_is_auto(&surround->margin[LXB_STYLE_EDGE_LEFT]);
    }

    if (margin != NULL) {
        margin->top =
            layout_reflow_edge_value(&surround->margin[LXB_STYLE_EDGE_TOP],
                                     percent_basis);
        margin->right =
            layout_reflow_edge_value(&surround->margin[LXB_STYLE_EDGE_RIGHT],
                                     percent_basis);
        margin->bottom =
            layout_reflow_edge_value(&surround->margin[LXB_STYLE_EDGE_BOTTOM],
                                     percent_basis);
        margin->left =
            layout_reflow_edge_value(&surround->margin[LXB_STYLE_EDGE_LEFT],
                                     percent_basis);
    }

    if (padding != NULL) {
        padding->top =
            layout_reflow_edge_value(&surround->padding[LXB_STYLE_EDGE_TOP],
                                     percent_basis);
        padding->right =
            layout_reflow_edge_value(&surround->padding[LXB_STYLE_EDGE_RIGHT],
                                     percent_basis);
        padding->bottom =
            layout_reflow_edge_value(&surround->padding[LXB_STYLE_EDGE_BOTTOM],
                                     percent_basis);
        padding->left =
            layout_reflow_edge_value(&surround->padding[LXB_STYLE_EDGE_LEFT],
                                     percent_basis);
    }

    if (border != NULL) {
        border->top =
            layout_reflow_border_width(&surround->border[LXB_STYLE_EDGE_TOP]);
        border->right =
            layout_reflow_border_width(&surround->border[LXB_STYLE_EDGE_RIGHT]);
        border->bottom =
            layout_reflow_border_width(
                &surround->border[LXB_STYLE_EDGE_BOTTOM]);
        border->left =
            layout_reflow_border_width(&surround->border[LXB_STYLE_EDGE_LEFT]);
    }
}

static layout_size_t
layout_reflow_default_object_size(const layout_reflow_context_t *ctx,
                                  layout_size_t containing_size)
{
    layout_size_t default_size = {0.0, 0.0};

    if (ctx != NULL && ctx->builder != NULL) {
        default_size = ctx->builder->default_object_size;
    }

    if (default_size.width <= 0.0) {
        default_size.width = containing_size.width;
    }

    if (default_size.height <= 0.0) {
        default_size.height = 0.0;
    }

    return default_size;
}

static bool
layout_reflow_box_value(const lxb_style_computed_value_t *value,
                        double percent_basis,
                        double *out_value)
{
    if (value == NULL || out_value == NULL
        || value->type == LXB_STYLE_COMPUTED_VALUE_AUTO
        || value->type == LXB_STYLE_COMPUTED_VALUE_NONE) {
        return false;
    }

    return layout_reflow_length_percentage(value, percent_basis, out_value);
}

static bool
layout_reflow_min_value(const lxb_style_computed_value_t *value,
                        double percent_basis,
                        double *out_value)
{
    if (!layout_reflow_box_value(value, percent_basis, out_value)) {
        *out_value = 0.0;
        return false;
    }

    return true;
}

static bool
layout_reflow_max_value(const lxb_style_computed_value_t *value,
                        double percent_basis,
                        double *out_value)
{
    if (!layout_reflow_box_value(value, percent_basis, out_value)) {
        return false;
    }

    return true;
}

static double
layout_reflow_apply_min_max(double value, double min_value,
                            bool has_max_value, double max_value)
{
    if (has_max_value && max_value < value) {
        value = max_value;
    }

    if (min_value > value) {
        value = min_value;
    }

    return value;
}

static double
layout_reflow_collapse_margins(double first, double second)
{
    if (first >= 0.0 && second >= 0.0) {
        return first > second ? first : second;
    }

    if (first <= 0.0 && second <= 0.0) {
        return first < second ? first : second;
    }

    return first + second;
}

static double
layout_reflow_measure_adjoining_margin_top(
    const layout_reflow_measure_t *measure)
{
    if (measure == NULL) {
        return 0.0;
    }

    return measure->has_adjoining_margin_top
        ? measure->adjoining_margin_top : measure->margin.top;
}

static double
layout_reflow_measure_adjoining_margin_bottom(
    const layout_reflow_measure_t *measure)
{
    if (measure == NULL) {
        return 0.0;
    }

    return measure->has_adjoining_margin_bottom
        ? measure->adjoining_margin_bottom : measure->margin.bottom;
}

static bool
layout_reflow_overflow_creates_bfc(const lxb_style_computed_non_inherited_t *ni)
{
    if (ni == NULL) {
        return false;
    }

    return ni->overflow_x != LXB_CSS_OVERFLOW_X_VISIBLE
        || ni->overflow_y != LXB_CSS_OVERFLOW_Y_VISIBLE;
}

static bool
layout_reflow_object_is_fixed(layout_object_t *object)
{
    layout_object_state_t state;

    if (object == NULL || layout_object_state(object, &state) != LXB_STATUS_OK) {
        return false;
    }

    return state.position == LXB_CSS_POSITION_FIXED;
}

static bool
layout_reflow_object_is_absolute(layout_object_t *object)
{
    layout_object_state_t state;

    if (object == NULL || layout_object_state(object, &state) != LXB_STATUS_OK) {
        return false;
    }

    return state.position == LXB_CSS_POSITION_ABSOLUTE;
}

static bool
layout_reflow_object_is_relative(layout_object_t *object)
{
    layout_object_state_t state;

    if (object == NULL || layout_object_state(object, &state) != LXB_STATUS_OK) {
        return false;
    }

    return state.position == LXB_CSS_POSITION_RELATIVE;
}

static bool
layout_reflow_object_is_out_of_flow(layout_object_t *object)
{
    return layout_reflow_object_is_absolute(object)
        || layout_reflow_object_is_fixed(object);
}

static layout_reflow_float_side_t
layout_reflow_object_float_side(layout_object_t *object)
{
    const lxb_style_computed_t *style;
    lxb_css_float_type_t float_type;

    if (object == NULL) {
        return LAYOUT_REFLOW_FLOAT_NONE;
    }

    style = layout_object_style(object);
    if (style == NULL || style->non_inherited == NULL
        || layout_reflow_object_is_out_of_flow(object)) {
        return LAYOUT_REFLOW_FLOAT_NONE;
    }

    float_type = style->non_inherited->floatp;
    if (float_type == LXB_CSS_FLOAT_LEFT
        || float_type == LXB_CSS_FLOAT_INLINE_START
        || float_type == LXB_CSS_FLOAT_START) {
        return LAYOUT_REFLOW_FLOAT_LEFT;
    }

    if (float_type == LXB_CSS_FLOAT_RIGHT
        || float_type == LXB_CSS_FLOAT_INLINE_END
        || float_type == LXB_CSS_FLOAT_END) {
        return LAYOUT_REFLOW_FLOAT_RIGHT;
    }

    return LAYOUT_REFLOW_FLOAT_NONE;
}

static layout_reflow_float_side_t
layout_reflow_object_clear_side(layout_object_t *object)
{
    const lxb_style_computed_t *style;
    lxb_css_clear_type_t clear_type;

    if (object == NULL) {
        return LAYOUT_REFLOW_FLOAT_NONE;
    }

    style = layout_object_style(object);
    if (style == NULL || style->non_inherited == NULL) {
        return LAYOUT_REFLOW_FLOAT_NONE;
    }

    clear_type = style->non_inherited->clear;
    if (clear_type == LXB_CSS_CLEAR_LEFT
        || clear_type == LXB_CSS_CLEAR_INLINE_START) {
        return LAYOUT_REFLOW_FLOAT_LEFT;
    }

    if (clear_type == LXB_CSS_CLEAR_RIGHT
        || clear_type == LXB_CSS_CLEAR_INLINE_END) {
        return LAYOUT_REFLOW_FLOAT_RIGHT;
    }

    if (clear_type == LXB_CSS_CLEAR_BLOCK_START
        || clear_type == LXB_CSS_CLEAR_TOP) {
        return LAYOUT_REFLOW_FLOAT_BOTH;
    }

    return LAYOUT_REFLOW_FLOAT_NONE;
}

static bool
layout_reflow_object_is_block_formatting_root(layout_object_t *object)
{
    const lxb_style_computed_t *style;
    const lxb_style_computed_non_inherited_t *ni;

    if (object == NULL) {
        return false;
    }

    if (layout_reflow_object_is_out_of_flow(object)
        || layout_reflow_object_float_side(object) != LAYOUT_REFLOW_FLOAT_NONE) {
        return true;
    }

    style = layout_object_style(object);
    if (style == NULL || style->non_inherited == NULL) {
        return false;
    }

    ni = style->non_inherited;
    return ni->display_inside == LXB_CSS_DISPLAY_FLOW_ROOT
        || ni->display_inside == LXB_CSS_DISPLAY_TABLE
        || ni->display_inside == LXB_CSS_DISPLAY_FLEX
        || ni->display_inside == LXB_CSS_DISPLAY_GRID
        || layout_reflow_overflow_creates_bfc(ni);
}

static int
layout_reflow_object_stacking_order(layout_object_t *object)
{
    layout_object_state_t state;

    if (object == NULL || layout_object_state(object, &state) != LXB_STATUS_OK) {
        return 0;
    }

    return state.z_index;
}

static layout_fragment_box_type_t
layout_reflow_object_box_type(layout_object_t *object)
{
    if (layout_reflow_object_is_out_of_flow(object)) {
        return LAYOUT_FRAGMENT_BOX_OUT_OF_FLOW_POSITIONED;
    }

    if (layout_reflow_object_float_side(object) != LAYOUT_REFLOW_FLOAT_NONE) {
        return LAYOUT_FRAGMENT_BOX_FLOATING;
    }

    if (layout_reflow_object_is_block_formatting_root(object)) {
        return LAYOUT_FRAGMENT_BOX_BLOCK_FLOW_ROOT;
    }

    return LAYOUT_FRAGMENT_BOX_NORMAL;
}

static layout_transform_t
layout_reflow_relative_transform(layout_object_t *object,
                                 layout_size_t containing_size)
{
    layout_reflow_insets_t insets;
    layout_transform_t transform = layout_algorithm_identity_transform;

    if (!layout_reflow_object_is_relative(object)) {
        return transform;
    }

    insets = layout_reflow_insets_from_style(layout_object_style(object),
                                             containing_size);
    if (!insets.has_explicit) {
        return transform;
    }

    if (!insets.left_auto) {
        transform.e = insets.left;
    }
    else if (!insets.right_auto) {
        transform.e = -insets.right;
    }

    if (!insets.top_auto) {
        transform.f = insets.top;
    }
    else if (!insets.bottom_auto) {
        transform.f = -insets.bottom;
    }

    return transform;
}

static const layout_reflow_ancestor_t *
layout_reflow_find_ancestor(const layout_reflow_ancestor_t *ancestor,
                            layout_object_t *object)
{
    for (const layout_reflow_ancestor_t *current = ancestor; current != NULL;
         current = current->parent) {
        if (current->object == object) {
            return current;
        }
    }

    return NULL;
}

static bool
layout_reflow_offset_from_ancestor(const layout_reflow_ancestor_t *ancestor,
                                   layout_object_t *object,
                                   layout_point_t origin,
                                   layout_object_t *target,
                                   layout_point_t *out_offset)
{
    const layout_reflow_ancestor_t *target_ancestor;
    layout_point_t offset = origin;

    if (out_offset == NULL) {
        return false;
    }

    if (object == target) {
        out_offset->x = 0.0;
        out_offset->y = 0.0;
        return true;
    }

    target_ancestor = layout_reflow_find_ancestor(ancestor, target);
    if (target_ancestor == NULL) {
        return false;
    }

    offset.x -= target_ancestor->origin.x;
    offset.y -= target_ancestor->origin.y;
    *out_offset = offset;
    return true;
}

static layout_point_t
layout_reflow_positioned_offset(layout_object_t *object,
                                const layout_reflow_ancestor_t *containing,
                                const layout_reflow_measure_t *measure,
                                layout_point_t static_offset)
{
    layout_reflow_insets_t insets;
    layout_point_t offset = static_offset;

    if (object == NULL || containing == NULL || measure == NULL) {
        return offset;
    }

    insets = layout_reflow_insets_from_style(layout_object_style(object),
                                             containing->padding_box_size);

    if (!insets.has_explicit) {
        return offset;
    }

    if (!insets.left_auto) {
        offset.x = containing->padding_box_offset.x + insets.left
            + measure->margin.left;
    }
    else if (!insets.right_auto) {
        offset.x = containing->padding_box_offset.x
            + containing->padding_box_size.width - insets.right
            - measure->margin.right - measure->border_size.width;
    }

    if (!insets.top_auto) {
        offset.y = containing->padding_box_offset.y + insets.top
            + measure->margin.top;
    }
    else if (!insets.bottom_auto) {
        offset.y = containing->padding_box_offset.y
            + containing->padding_box_size.height - insets.bottom
            - measure->margin.bottom - measure->border_size.height;
    }

    return offset;
}

static void
layout_reflow_resolve_horizontal_auto_margins(layout_reflow_measure_t *measure,
                                             layout_size_t containing_size)
{
    double remaining;

    if (measure == NULL) {
        return;
    }

    if (!measure->margin_auto.left && !measure->margin_auto.right) {
        return;
    }

    remaining = containing_size.width - measure->border_size.width;
    if (!measure->margin_auto.left) {
        remaining -= measure->margin.left;
    }
    if (!measure->margin_auto.right) {
        remaining -= measure->margin.right;
    }

    if (remaining < 0.0) {
        remaining = 0.0;
    }

    if (measure->margin_auto.left && measure->margin_auto.right) {
        measure->margin.left = remaining / 2.0;
        measure->margin.right = remaining / 2.0;
    }
    else if (measure->margin_auto.left) {
        measure->margin.left = remaining;
    }
    else if (measure->margin_auto.right) {
        measure->margin.right = remaining;
    }
}

static bool
layout_reflow_collapses_child_margins(const layout_reflow_input_t *input,
                                      const layout_reflow_measure_t *measure)
{
    if (input == NULL || measure == NULL || !input->has_parent
        || measure->explicit_height
        || layout_reflow_object_is_block_formatting_root(input->object)) {
        return false;
    }

    return measure->border.top == 0.0 && measure->border.bottom == 0.0
        && measure->padding.top == 0.0 && measure->padding.bottom == 0.0;
}

static bool
layout_reflow_collapses_own_margins(const layout_reflow_input_t *input,
                                    const layout_reflow_measure_t *measure)
{
    if (input == NULL || measure == NULL || !input->has_parent
        || measure->explicit_height
        || layout_reflow_object_is_block_formatting_root(input->object)) {
        return false;
    }

    return measure->border.top == 0.0 && measure->border.bottom == 0.0
        && measure->padding.top == 0.0 && measure->padding.bottom == 0.0
        && measure->border_size.height == 0.0;
}

static double
layout_reflow_float_margin_width(const layout_reflow_measure_t *measure)
{
    return measure == NULL ? 0.0
        : measure->margin.left + measure->border_size.width
            + measure->margin.right;
}

static double
layout_reflow_active_float_width(const layout_reflow_float_state_t *floats,
                                 layout_reflow_float_side_t side,
                                 double block_offset)
{
    double width = 0.0;

    if (floats == NULL) {
        return 0.0;
    }

    for (size_t i = 0; i < floats->count; i++) {
        const layout_reflow_float_entry_t *entry = &floats->entries[i];

        if ((entry->side & side) != 0 && block_offset >= entry->top
            && block_offset < entry->bottom) {
            width += entry->width;
        }
    }

    return width;
}

static double
layout_reflow_next_float_boundary(const layout_reflow_float_state_t *floats,
                                  double block_offset)
{
    double boundary = 0.0;

    if (floats == NULL) {
        return 0.0;
    }

    for (size_t i = 0; i < floats->count; i++) {
        const layout_reflow_float_entry_t *entry = &floats->entries[i];

        if (block_offset >= entry->top && block_offset < entry->bottom
            && (boundary == 0.0 || entry->bottom < boundary)) {
            boundary = entry->bottom;
        }
    }

    return boundary;
}

static double
layout_reflow_available_inline_size(const layout_reflow_float_state_t *floats,
                                    double containing_width,
                                    double block_offset)
{
    double left_width = layout_reflow_active_float_width(
        floats, LAYOUT_REFLOW_FLOAT_LEFT, block_offset);
    double right_width = layout_reflow_active_float_width(
        floats, LAYOUT_REFLOW_FLOAT_RIGHT, block_offset);

    return layout_reflow_non_negative(containing_width - left_width
                                      - right_width);
}

static double
layout_reflow_find_float_fit_offset(const layout_reflow_float_state_t *floats,
                                    double containing_width,
                                    double block_offset,
                                    double margin_width)
{
    while (margin_width
           > layout_reflow_available_inline_size(floats, containing_width,
                                                 block_offset)) {
        double next_boundary =
            layout_reflow_next_float_boundary(floats, block_offset);

        if (next_boundary <= block_offset) {
            break;
        }

        block_offset = next_boundary;
    }

    return block_offset;
}

static double
layout_reflow_find_normal_fit_offset(const layout_reflow_float_state_t *floats,
                                     double containing_width,
                                     double block_offset,
                                     const layout_reflow_measure_t *measure)
{
    return layout_reflow_find_float_fit_offset(
        floats, containing_width, block_offset,
        layout_reflow_float_margin_width(measure));
}

static layout_point_t
layout_reflow_float_offset(const layout_reflow_ancestor_t *ancestor,
                           const layout_reflow_float_state_t *floats,
                           layout_reflow_float_side_t float_side,
                           const layout_reflow_measure_t *measure,
                           double block_offset)
{
    layout_point_t offset = {0.0, 0.0};

    if (ancestor == NULL || measure == NULL) {
        return offset;
    }

    if ((float_side & LAYOUT_REFLOW_FLOAT_RIGHT) != 0) {
        offset.x = ancestor->content_offset.x + ancestor->content_size.width
            - layout_reflow_active_float_width(
                floats, LAYOUT_REFLOW_FLOAT_RIGHT, block_offset)
            - measure->margin.right - measure->border_size.width;
    }
    else {
        offset.x = ancestor->content_offset.x
            + layout_reflow_active_float_width(
                floats, LAYOUT_REFLOW_FLOAT_LEFT, block_offset)
            + measure->margin.left;
    }

    offset.y = ancestor->content_offset.y + block_offset;
    return offset;
}

static layout_point_t
layout_reflow_normal_offset(const layout_reflow_ancestor_t *ancestor,
                            const layout_reflow_float_state_t *floats,
                            const layout_reflow_measure_t *measure,
                            double block_offset)
{
    layout_point_t offset = {0.0, 0.0};

    if (ancestor == NULL || measure == NULL) {
        return offset;
    }

    offset.x = ancestor->content_offset.x
        + layout_reflow_active_float_width(floats, LAYOUT_REFLOW_FLOAT_LEFT,
                                           block_offset)
        + measure->margin.left;
    offset.y = ancestor->content_offset.y + block_offset;
    return offset;
}

static double
layout_reflow_clearance_offset(const layout_reflow_float_state_t *floats,
                               layout_reflow_float_side_t clear_side)
{
    double offset = 0.0;

    if (floats == NULL) {
        return 0.0;
    }

    for (size_t i = 0; i < floats->count; i++) {
        const layout_reflow_float_entry_t *entry = &floats->entries[i];

        if ((entry->side & clear_side) != 0 && entry->bottom > offset) {
            offset = entry->bottom;
        }
    }

    return offset;
}

static bool
layout_reflow_apply_clearance(const layout_reflow_float_state_t *floats,
                              layout_reflow_float_side_t clear_side,
                              double *block_offset)
{
    double clearance = layout_reflow_clearance_offset(floats, clear_side);

    if (block_offset == NULL || clear_side == LAYOUT_REFLOW_FLOAT_NONE) {
        return false;
    }

    if (clearance > *block_offset) {
        *block_offset = clearance;
        return true;
    }

    return false;
}

static void
layout_reflow_add_float(layout_reflow_float_state_t *floats,
                        layout_reflow_float_side_t float_side,
                        const layout_reflow_measure_t *measure,
                        double block_offset)
{
    double margin_width;
    double margin_bottom;
    layout_reflow_float_entry_t *entry;

    if (floats == NULL || measure == NULL
        || float_side == LAYOUT_REFLOW_FLOAT_NONE) {
        return;
    }

    if (floats->count >= LAYOUT_REFLOW_MAX_FLOATS) {
        return;
    }

    margin_width = layout_reflow_float_margin_width(measure);
    margin_bottom = block_offset + measure->border_size.height
        + measure->margin.bottom;

    entry = &floats->entries[floats->count++];
    entry->side = (float_side & LAYOUT_REFLOW_FLOAT_RIGHT) != 0
        ? LAYOUT_REFLOW_FLOAT_RIGHT : LAYOUT_REFLOW_FLOAT_LEFT;
    entry->top = block_offset;
    entry->bottom = margin_bottom;
    entry->width = margin_width;
}

static void
layout_reflow_add_offset_float_state(layout_reflow_float_state_t *floats,
                                     const layout_reflow_float_state_t *source,
                                     double block_offset)
{
    if (floats == NULL || source == NULL) {
        return;
    }

    for (size_t i = 0; i < source->count
         && floats->count < LAYOUT_REFLOW_MAX_FLOATS; i++) {
        layout_reflow_float_entry_t *entry = &floats->entries[floats->count++];

        *entry = source->entries[i];
        entry->top += block_offset;
        entry->bottom += block_offset;
    }
}

static double
layout_reflow_float_escape_offset(const layout_reflow_measure_t *measure,
                                  double block_offset)
{
    if (measure == NULL) {
        return block_offset;
    }

    return block_offset + measure->border.top + measure->padding.top;
}

static double
layout_reflow_float_extent(const layout_reflow_float_state_t *floats)
{
    double extent = 0.0;

    if (floats == NULL) {
        return 0.0;
    }

    for (size_t i = 0; i < floats->count; i++) {
        if (floats->entries[i].bottom > extent) {
            extent = floats->entries[i].bottom;
        }
    }

    return extent;
}

static double
layout_reflow_normal_block_offset(double block_cursor,
                                  double previous_margin_bottom,
                                  double child_margin_top,
                                  bool has_previous_normal,
                                  bool collapse_parent_child,
                                  double default_block_gap)
{
    if (has_previous_normal) {
        return block_cursor + default_block_gap
            + layout_reflow_collapse_margins(previous_margin_bottom,
                                             child_margin_top);
    }

    if (collapse_parent_child) {
        return block_cursor;
    }

    return block_cursor + child_margin_top;
}

static double
layout_reflow_float_block_offset(double block_cursor,
                                 double previous_margin_bottom,
                                 double child_margin_top,
                                 bool has_previous_normal,
                                 bool collapse_parent_child,
                                 double default_block_gap)
{
    (void) collapse_parent_child;

    if (has_previous_normal) {
        return block_cursor + default_block_gap + previous_margin_bottom
            + child_margin_top;
    }

    return block_cursor + child_margin_top;
}

static lxb_status_t
layout_reflow_measure_object(const layout_reflow_context_t *ctx,
                             const layout_reflow_input_t *input,
                             layout_reflow_measure_t *out_measure);

static lxb_status_t
layout_reflow_child_used_block_size(const layout_reflow_context_t *ctx,
                                    const layout_reflow_input_t *input,
                                    const layout_reflow_measure_t *measure,
                                    double *out_block_size,
                                    layout_reflow_collapsed_child_margins_t
                                        *out_collapsed_margins,
                                    layout_reflow_float_state_t
                                        *out_escaped_floats)
{
    layout_object_child_list_t *children;
    layout_reflow_float_state_t floats;
    double block_cursor = 0.0;
    double previous_margin_bottom = 0.0;
    bool has_previous_normal = false;
    bool collapse_parent_child;
    bool leading_margins_adjoining;
    bool has_leading_margin = false;
    double leading_margin = 0.0;
    double normal_extent = 0.0;
    double float_extent = 0.0;
    bool contains_internal_floats;

    if (ctx == NULL || input == NULL || measure == NULL
        || out_block_size == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    if (out_escaped_floats != NULL) {
        memset(out_escaped_floats, 0, sizeof(*out_escaped_floats));
    }

    children = layout_tree_children(ctx->tree, input->object);
    if (children == NULL) {
        *out_block_size = 0.0;
        if (out_collapsed_margins != NULL) {
            memset(out_collapsed_margins, 0,
                   sizeof(*out_collapsed_margins));
        }
        return LXB_STATUS_OK;
    }

    memset(&floats, 0, sizeof(floats));
    if (out_collapsed_margins != NULL) {
        memset(out_collapsed_margins, 0, sizeof(*out_collapsed_margins));
    }
    collapse_parent_child =
        layout_reflow_collapses_child_margins(input, measure);
    leading_margins_adjoining = collapse_parent_child;
    contains_internal_floats = !input->has_parent
        || layout_reflow_object_is_block_formatting_root(input->object);

    for (layout_object_t *child = children->first_child; child != NULL;
         child = layout_object_next_sibling(child)) {
        layout_reflow_input_t child_input;
        layout_reflow_measure_t child_measure;
        layout_reflow_float_side_t float_side =
            layout_reflow_object_float_side(child);
        layout_reflow_float_side_t clear_side =
            layout_reflow_object_clear_side(child);
        double child_block_offset;
        double child_leading_margin;
        double child_margin_top;
        double child_margin_bottom;
        bool had_clearance;
        bool use_previous_margin;
        bool is_oof = layout_reflow_object_is_out_of_flow(child);
        lxb_status_t status;

        if (is_oof) {
            continue;
        }

        child_input.object = child;
        child_input.containing_size = measure->content_size;
        child_input.origin.x = 0.0;
        child_input.origin.y = 0.0;
        child_input.ancestor = input->ancestor;
        child_input.has_parent = true;

        status = layout_reflow_measure_object(ctx, &child_input,
                                              &child_measure);
        if (status != LXB_STATUS_OK) {
            return status;
        }

        child_margin_top =
            layout_reflow_measure_adjoining_margin_top(&child_measure);
        child_margin_bottom =
            layout_reflow_measure_adjoining_margin_bottom(&child_measure);

        if (float_side != LAYOUT_REFLOW_FLOAT_NONE) {
            child_block_offset = layout_reflow_float_block_offset(
                block_cursor, previous_margin_bottom,
                child_margin_top, has_previous_normal,
                collapse_parent_child, ctx->default_block_gap);
            (void) layout_reflow_apply_clearance(&floats, clear_side,
                                                 &child_block_offset);
            child_block_offset = layout_reflow_find_float_fit_offset(
                &floats, measure->content_size.width, child_block_offset,
                layout_reflow_float_margin_width(&child_measure));
            layout_reflow_add_float(&floats, float_side, &child_measure,
                                    child_block_offset);
            continue;
        }

        use_previous_margin =
            has_previous_normal && !leading_margins_adjoining;
        child_block_offset = layout_reflow_normal_block_offset(
            block_cursor, previous_margin_bottom, child_margin_top,
            use_previous_margin, collapse_parent_child,
            ctx->default_block_gap);
        had_clearance = layout_reflow_apply_clearance(&floats, clear_side,
                                                      &child_block_offset);
        child_block_offset = layout_reflow_find_normal_fit_offset(
            &floats, measure->content_size.width, child_block_offset,
            &child_measure);

        child_leading_margin = child_measure.collapses_own_margins
            ? child_measure.collapsed_margin : child_margin_top;
        if (had_clearance) {
            leading_margins_adjoining = false;
        }
        else if (leading_margins_adjoining) {
            leading_margin = has_leading_margin
                ? layout_reflow_collapse_margins(leading_margin,
                                                 child_leading_margin)
                : child_leading_margin;
            has_leading_margin = true;
            if (!child_measure.collapses_own_margins) {
                leading_margins_adjoining = false;
            }
        }

        if (child_measure.collapses_own_margins && !had_clearance) {
            previous_margin_bottom = has_previous_normal
                ? layout_reflow_collapse_margins(previous_margin_bottom,
                                                 child_measure.collapsed_margin)
                : child_measure.collapsed_margin;
        }
        else {
            block_cursor = child_block_offset + child_measure.border_size.height;
            previous_margin_bottom = child_margin_bottom;
        }
        has_previous_normal = true;
        layout_reflow_add_offset_float_state(
            &floats, &child_measure.escaped_floats,
            layout_reflow_float_escape_offset(&child_measure,
                                              child_block_offset));
    }

    if (has_previous_normal) {
        normal_extent = block_cursor;
        if (!collapse_parent_child) {
            normal_extent += previous_margin_bottom;
        }
    }

    if (collapse_parent_child && has_previous_normal
        && out_collapsed_margins != NULL) {
        out_collapsed_margins->has_top = has_leading_margin;
        out_collapsed_margins->top = leading_margin;
        out_collapsed_margins->has_bottom = true;
        out_collapsed_margins->bottom = previous_margin_bottom;
    }

    float_extent = layout_reflow_float_extent(&floats);
    if (contains_internal_floats) {
        *out_block_size = normal_extent > float_extent
            ? normal_extent : float_extent;
    }
    else {
        *out_block_size = normal_extent;
        if (out_escaped_floats != NULL) {
            *out_escaped_floats = floats;
        }
    }
    return LXB_STATUS_OK;
}

static lxb_status_t
layout_reflow_measure_object(const layout_reflow_context_t *ctx,
                             const layout_reflow_input_t *input,
                             layout_reflow_measure_t *out_measure)
{
    const lxb_style_computed_t *style;
    lxb_css_box_sizing_type_t box_sizing = LXB_CSS_BOX_SIZING_CONTENT_BOX;
    layout_reflow_measure_t measure;
    layout_size_t default_size;
    double width_px = 0.0;
    double height_px = 0.0;
    double min_width = 0.0;
    double min_height = 0.0;
    double max_width = 0.0;
    double max_height = 0.0;
    bool has_max_width = false;
    bool has_max_height = false;
    double child_block_size = 0.0;
    layout_reflow_collapsed_child_margins_t collapsed_child_margins;
    double horizontal_edges;
    double vertical_edges;
    lxb_status_t status;

    if (ctx == NULL || input == NULL || input->object == NULL
        || out_measure == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    memset(&measure, 0, sizeof(measure));
    memset(&collapsed_child_margins, 0, sizeof(collapsed_child_margins));
    style = layout_object_style(input->object);
    layout_reflow_edges_from_style(style, input->containing_size.width,
                                   &measure.raw_margin, &measure.margin_auto,
                                   &measure.padding, &measure.border);
    measure.margin = measure.raw_margin;

    if (style != NULL && style->non_inherited != NULL) {
        box_sizing = style->non_inherited->box_sizing;
    }

    default_size = layout_reflow_default_object_size(ctx,
                                                     input->containing_size);
    measure.border_size = default_size;

    if (style != NULL && style->box != NULL) {
        measure.explicit_width =
            layout_reflow_box_value(&style->box->width,
                                    input->containing_size.width, &width_px);
        measure.explicit_height =
            layout_reflow_box_value(&style->box->height,
                                    input->containing_size.height,
                                    &height_px);
        layout_reflow_min_value(&style->box->min_width,
                                input->containing_size.width, &min_width);
        layout_reflow_min_value(&style->box->min_height,
                                input->containing_size.height, &min_height);
        has_max_width =
            layout_reflow_max_value(&style->box->max_width,
                                    input->containing_size.width, &max_width);
        has_max_height =
            layout_reflow_max_value(&style->box->max_height,
                                    input->containing_size.height,
                                    &max_height);
    }

    if (has_max_width && min_width > max_width) {
        max_width = min_width;
    }

    if (has_max_height && min_height > max_height) {
        max_height = min_height;
    }

    horizontal_edges = measure.border.left + measure.padding.left
        + measure.padding.right + measure.border.right;
    vertical_edges = measure.border.top + measure.padding.top
        + measure.padding.bottom + measure.border.bottom;

    if (measure.explicit_width) {
        width_px = layout_reflow_apply_min_max(width_px, min_width,
                                               has_max_width, max_width);
        measure.border_size.width = box_sizing == LXB_CSS_BOX_SIZING_BORDER_BOX
            ? width_px : width_px + horizontal_edges;
    }
    else {
        double available_width = input->containing_size.width
            - measure.margin.left - measure.margin.right;
        if (default_size.width > 0.0
            && default_size.width < input->containing_size.width) {
            available_width = default_size.width;
        }
        measure.border_size.width = available_width;
    }

    measure.border_size.width =
        layout_reflow_non_negative(measure.border_size.width);
    layout_reflow_resolve_horizontal_auto_margins(&measure,
                                                  input->containing_size);
    measure.content_size.width =
        layout_reflow_non_negative(measure.border_size.width
                                   - horizontal_edges);

    if (measure.explicit_height) {
        double ignored_child_block_size = 0.0;

        height_px = layout_reflow_apply_min_max(height_px, min_height,
                                                has_max_height, max_height);
        measure.border_size.height =
            box_sizing == LXB_CSS_BOX_SIZING_BORDER_BOX
                ? height_px : height_px + vertical_edges;
        status = layout_reflow_child_used_block_size(ctx, input, &measure,
                                                     &ignored_child_block_size,
                                                     NULL,
                                                     &measure.escaped_floats);
        if (status != LXB_STATUS_OK) {
            return status;
        }
    }
    else {
        status = layout_reflow_child_used_block_size(ctx, input, &measure,
                                                     &child_block_size,
                                                     &collapsed_child_margins,
                                                     &measure.escaped_floats);
        if (status != LXB_STATUS_OK) {
            return status;
        }

        measure.border_size.height = vertical_edges + child_block_size;
        if (box_sizing == LXB_CSS_BOX_SIZING_CONTENT_BOX) {
            child_block_size = layout_reflow_apply_min_max(
                child_block_size, min_height, has_max_height, max_height);
            measure.border_size.height = vertical_edges + child_block_size;
        }
        else {
            measure.border_size.height = layout_reflow_apply_min_max(
                measure.border_size.height, min_height, has_max_height,
                max_height);
        }
        if (measure.border_size.height <= 0.0 && default_size.height > 0.0) {
            measure.border_size.height = default_size.height;
        }
    }

    measure.border_size.height =
        layout_reflow_non_negative(measure.border_size.height);
    measure.content_size.height =
        layout_reflow_non_negative(measure.border_size.height
                                   - vertical_edges);
    if (collapsed_child_margins.has_top) {
        measure.has_adjoining_margin_top = true;
        measure.adjoining_margin_top =
            layout_reflow_collapse_margins(measure.margin.top,
                                           collapsed_child_margins.top);
    }
    if (collapsed_child_margins.has_bottom) {
        measure.has_adjoining_margin_bottom = true;
        measure.adjoining_margin_bottom =
            layout_reflow_collapse_margins(measure.margin.bottom,
                                           collapsed_child_margins.bottom);
    }
    measure.collapses_own_margins =
        layout_reflow_collapses_own_margins(input, &measure);
    measure.collapsed_margin = measure.collapses_own_margins
        ? layout_reflow_collapse_margins(
            layout_reflow_measure_adjoining_margin_top(&measure),
            layout_reflow_measure_adjoining_margin_bottom(&measure))
        : layout_reflow_measure_adjoining_margin_bottom(&measure);

    *out_measure = measure;
    return LXB_STATUS_OK;
}

static lxb_status_t
layout_reflow_build_object(const layout_reflow_context_t *ctx,
                           const layout_reflow_input_t *input,
                           layout_fragment_t **out_fragment)
{
    layout_fragment_init_t init;
    layout_fragment_t *fragment;
    layout_reflow_measure_t measure;
    layout_reflow_ancestor_t current_ancestor;
    layout_object_child_list_t *children;
    layout_reflow_float_state_t floats;
    double block_cursor = 0.0;
    double previous_margin_bottom = 0.0;
    bool has_previous_normal = false;
    bool collapse_parent_child;
    bool leading_margins_adjoining;
    lxb_status_t status;

    if (out_fragment != NULL) {
        *out_fragment = NULL;
    }

    if (ctx == NULL || input == NULL || input->object == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    status = layout_reflow_measure_object(ctx, input, &measure);
    if (status != LXB_STATUS_OK) {
        return status;
    }

    memset(&init, 0, sizeof(init));
    init.object = input->object;
    init.size = measure.border_size;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = layout_reflow_object_box_type(input->object);
    init.stacking_order = layout_reflow_object_stacking_order(input->object);
    init.transform = layout_reflow_relative_transform(input->object,
                                                      input->containing_size);
    init.margin.top = measure.margin.top;
    init.margin.right = measure.margin.right;
    init.margin.bottom = measure.margin.bottom;
    init.margin.left = measure.margin.left;
    init.border.top = measure.border.top;
    init.border.right = measure.border.right;
    init.border.bottom = measure.border.bottom;
    init.border.left = measure.border.left;
    init.padding.top = measure.padding.top;
    init.padding.right = measure.padding.right;
    init.padding.bottom = measure.padding.bottom;
    init.padding.left = measure.padding.left;
    init.border_sides = LAYOUT_BOX_SIDE_ALL;

    fragment = layout_fragment_create(ctx->result, &init);
    if (fragment == NULL) {
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    current_ancestor.object = input->object;
    current_ancestor.fragment = fragment;
    current_ancestor.origin = input->origin;
    current_ancestor.size = measure.border_size;
    current_ancestor.padding_box_offset.x = measure.border.left;
    current_ancestor.padding_box_offset.y = measure.border.top;
    current_ancestor.padding_box_size.width = layout_reflow_non_negative(
        measure.border_size.width - measure.border.left - measure.border.right);
    current_ancestor.padding_box_size.height = layout_reflow_non_negative(
        measure.border_size.height - measure.border.top
        - measure.border.bottom);
    current_ancestor.content_offset.x = measure.border.left
        + measure.padding.left;
    current_ancestor.content_offset.y = measure.border.top
        + measure.padding.top;
    current_ancestor.content_size = measure.content_size;
    current_ancestor.parent = input->ancestor;

    if (out_fragment != NULL) {
        *out_fragment = fragment;
    }

    children = layout_tree_children(ctx->tree, input->object);
    if (children == NULL) {
        return LXB_STATUS_OK;
    }

    memset(&floats, 0, sizeof(floats));
    collapse_parent_child =
        layout_reflow_collapses_child_margins(input, &measure);
    leading_margins_adjoining = collapse_parent_child;

    for (layout_object_t *child = children->first_child; child != NULL;
         child = layout_object_next_sibling(child)) {
        layout_reflow_input_t child_input;
        layout_reflow_measure_t child_measure;
        layout_fragment_t *child_fragment = NULL;
        layout_object_t *containing_block = NULL;
        const layout_reflow_ancestor_t *containing_ancestor = NULL;
        layout_size_t child_containing_size = measure.content_size;
        layout_point_t child_offset;
        layout_point_t child_origin;
        layout_point_t static_offset;
        double child_block_offset;
        double child_margin_top;
        double child_margin_bottom;
        bool had_clearance = false;
        bool use_previous_margin;
        layout_reflow_float_side_t float_side =
            layout_reflow_object_float_side(child);
        layout_reflow_float_side_t clear_side =
            layout_reflow_object_clear_side(child);
        bool has_positioned_inset = false;
        bool is_fixed = layout_reflow_object_is_fixed(child);
        bool is_absolute = layout_reflow_object_is_absolute(child);
        bool is_oof = is_fixed || is_absolute;

        if (is_oof) {
            containing_block = is_fixed
                ? layout_object_containing_block_for_fixed(child)
                : layout_object_containing_block_for_absolute(child);

            if (containing_block == NULL) {
                containing_block = input->object;
            }

            containing_ancestor =
                layout_reflow_find_ancestor(&current_ancestor,
                                            containing_block);
            if (containing_ancestor == NULL) {
                return LXB_STATUS_ERROR_NOT_EXISTS;
            }

            child_containing_size = containing_ancestor->padding_box_size;
            has_positioned_inset = layout_reflow_object_has_positioned_inset(
                child, child_containing_size);
        }

        child_input.object = child;
        child_input.containing_size = child_containing_size;
        child_input.origin.x = 0.0;
        child_input.origin.y = 0.0;
        child_input.ancestor = &current_ancestor;
        child_input.has_parent = true;

        status = layout_reflow_measure_object(ctx, &child_input,
                                              &child_measure);
        if (status != LXB_STATUS_OK) {
            return status;
        }

        child_margin_top = is_oof
            ? child_measure.margin.top
            : layout_reflow_measure_adjoining_margin_top(&child_measure);
        child_margin_bottom = is_oof
            ? child_measure.margin.bottom
            : layout_reflow_measure_adjoining_margin_bottom(&child_measure);

        if (float_side != LAYOUT_REFLOW_FLOAT_NONE) {
            child_block_offset = layout_reflow_float_block_offset(
                block_cursor, previous_margin_bottom,
                child_margin_top, has_previous_normal,
                collapse_parent_child, ctx->default_block_gap);
            (void) layout_reflow_apply_clearance(&floats, clear_side,
                                                 &child_block_offset);
            child_block_offset = layout_reflow_find_float_fit_offset(
                &floats, measure.content_size.width, child_block_offset,
                layout_reflow_float_margin_width(&child_measure));
            child_offset = layout_reflow_float_offset(
                &current_ancestor, &floats, float_side, &child_measure,
                child_block_offset);
        }
        else {
            use_previous_margin =
                has_previous_normal && !leading_margins_adjoining;
            child_block_offset = layout_reflow_normal_block_offset(
                block_cursor, previous_margin_bottom,
                child_margin_top, use_previous_margin,
                collapse_parent_child, ctx->default_block_gap);
            if (!is_oof) {
                had_clearance =
                    layout_reflow_apply_clearance(&floats, clear_side,
                                                  &child_block_offset);
            }
            child_block_offset = layout_reflow_find_normal_fit_offset(
                &floats, measure.content_size.width, child_block_offset,
                &child_measure);
            child_offset = layout_reflow_normal_offset(
                &current_ancestor, &floats, &child_measure,
                child_block_offset);
        }

        child_origin.x = input->origin.x + child_offset.x;
        child_origin.y = input->origin.y + child_offset.y;

        if (is_oof && has_positioned_inset) {
            if (!layout_reflow_offset_from_ancestor(&current_ancestor, child,
                                                    child_origin,
                                                    containing_block,
                                                    &static_offset)) {
                return LXB_STATUS_ERROR_NOT_EXISTS;
            }

            child_offset = layout_reflow_positioned_offset(
                child, containing_ancestor, &child_measure, static_offset);
            child_origin.x = containing_ancestor->origin.x + child_offset.x;
            child_origin.y = containing_ancestor->origin.y + child_offset.y;
        }

        child_input.origin = child_origin;

        status = layout_reflow_build_object(ctx, &child_input,
                                            &child_fragment);
        if (status != LXB_STATUS_OK) {
            return status;
        }

        if (child_fragment == NULL) {
            continue;
        }

        if (is_oof) {
            if (!has_positioned_inset
                && !layout_reflow_offset_from_ancestor(&current_ancestor,
                                                       child, child_origin,
                                                       containing_block,
                                                       &child_offset)) {
                return LXB_STATUS_ERROR_NOT_EXISTS;
            }

            status = layout_result_add_out_of_flow_positioned_in_fragment(
                ctx->result, child, containing_block,
                containing_ancestor->fragment, child_fragment, child_offset,
                is_fixed);
        }
        else {
            status = layout_fragment_append_child_with_placement(
                fragment, child_fragment, child_offset,
                float_side == LAYOUT_REFLOW_FLOAT_NONE
                    ? LAYOUT_FRAGMENT_PLACEMENT_NORMAL
                    : LAYOUT_FRAGMENT_PLACEMENT_FLOAT,
                layout_reflow_object_stacking_order(child));
            if (status == LXB_STATUS_OK) {
                if (float_side != LAYOUT_REFLOW_FLOAT_NONE) {
                    layout_reflow_add_float(&floats, float_side,
                                            &child_measure,
                                            child_block_offset);
                }
                else if (child_measure.collapses_own_margins
                         && !had_clearance) {
                    previous_margin_bottom = has_previous_normal
                        ? layout_reflow_collapse_margins(
                            previous_margin_bottom,
                            child_measure.collapsed_margin)
                        : child_measure.collapsed_margin;
                    has_previous_normal = true;
                }
                else {
                    leading_margins_adjoining = false;
                    block_cursor = child_block_offset
                        + child_measure.border_size.height;
                    previous_margin_bottom = child_margin_bottom;
                    has_previous_normal = true;
                }
                if (float_side == LAYOUT_REFLOW_FLOAT_NONE) {
                    layout_reflow_add_offset_float_state(
                        &floats, &child_measure.escaped_floats,
                        layout_reflow_float_escape_offset(&child_measure,
                                                          child_block_offset));
                }
            }
        }

        if (status != LXB_STATUS_OK) {
            return status;
        }
    }

    return LXB_STATUS_OK;
}

static void
layout_reflow_clear_object_subtree_needs_layout(layout_tree_t *tree,
                                                layout_object_t *object)
{
    layout_object_child_list_t *children;

    if (tree == NULL || object == NULL) {
        return;
    }

    children = layout_tree_children(tree, object);
    if (children != NULL) {
        for (layout_object_t *child = children->first_child; child != NULL;
             child = layout_object_next_sibling(child)) {
            layout_reflow_clear_object_subtree_needs_layout(tree, child);
        }
    }

    layout_object_clear_needs_layout(object);
}

lxb_status_t
layout_algorithm_build_fragments(layout_tree_t *tree,
                                 layout_result_t *result,
                                 const layout_fragment_builder_t *builder)
{
    layout_reflow_context_t ctx;
    layout_reflow_input_t input;
    layout_fragment_t *root_fragment = NULL;
    layout_object_t *root_object;
    layout_size_t viewport_size = {0.0, 0.0};
    layout_point_t root_origin = {0.0, 0.0};
    lxb_status_t status;

    if (tree == NULL || result == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    if (layout_result_is_frozen(result)) {
        return LXB_STATUS_ERROR_WRONG_STAGE;
    }

    if (layout_result_root_fragment(result) != NULL) {
        return LXB_STATUS_ERROR_WRONG_STAGE;
    }

    if (layout_tree_layout(tree) != layout_result_layout(result)) {
        return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    root_object = layout_tree_root_object(tree);
    if (root_object == NULL) {
        return LXB_STATUS_ERROR_WRONG_STAGE;
    }

    if (builder != NULL) {
        viewport_size = builder->viewport_size;
    }

    ctx.tree = tree;
    ctx.result = result;
    ctx.builder = builder;
    ctx.default_block_gap = builder == NULL ? 0.0 : builder->default_block_gap;

    input.object = root_object;
    input.containing_size = viewport_size;
    input.origin = root_origin;
    input.ancestor = NULL;
    input.has_parent = false;

    status = layout_reflow_build_object(&ctx, &input, &root_fragment);
    if (status != LXB_STATUS_OK) {
        return status;
    }

    status = layout_result_set_root_fragment(result, root_fragment);
    if (status == LXB_STATUS_OK) {
        layout_reflow_clear_object_subtree_needs_layout(tree, root_object);
    }

    return status;
}
