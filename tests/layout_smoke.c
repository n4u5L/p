#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "layout/layout.h"
#include "lexbor/css/property/const.h"
#include "lexbor/dom/interface.h"
#include "lexbor/dom/interfaces/element.h"

static int failed = 0;

static layout_point_t
make_point(double x, double y)
{
    layout_point_t point;

    point.x = x;
    point.y = y;

    return point;
}

static void
check(int ok, const char *msg)
{
    if (!ok) {
        failed = 1;
        printf("FAIL: %s\n", msg);
    }
}

static void
freeze_result(layout_result_t *result)
{
    check(layout_result_freeze(result) == LXB_STATUS_OK,
          "layout result freezes after fragment tree build");
    check(layout_result_is_frozen(result), "layout result reports frozen");
}

static bool
fragment_key_equal(layout_fragment_key_t left, layout_fragment_key_t right)
{
    return left.object_id == right.object_id
        && left.fragment_index == right.fragment_index
        && left.role == right.role
        && left.ordinal == right.ordinal;
}

static bool
fragment_key_empty(layout_fragment_key_t key)
{
    layout_fragment_key_t empty = {0, 0, 0, 0};

    return fragment_key_equal(key, empty);
}

static bool
near_double(double left, double right)
{
    double delta = left - right;

    return (delta < 0.0 ? -delta : delta) < 0.0001;
}

static lxb_style_computed_t *
make_style(void)
{
    lxb_style_computed_t *style = lxb_style_computed_create();

    if (style == NULL
        || lxb_style_computed_set_initial(style, 16.0, 19.2)
               != LXB_STATUS_OK) {
        return NULL;
    }

    return style;
}

static layout_object_t *
make_object(layout_t *layout)
{
    lxb_style_computed_t *style = make_style();
    layout_object_t *object = layout_object_create_anonymous(layout, style);

    if (style != NULL) {
        lxb_style_computed_unref(style);
    }

    return object;
}

static void
set_position(layout_object_t *object, lxb_css_position_type_t position)
{
    lxb_style_computed_t *style =
        (lxb_style_computed_t *) layout_object_style(object);
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "position style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);

    check(non_inherited != NULL, "style non-inherited group is mutable");
    if (non_inherited != NULL) {
        non_inherited->position = position;
    }
}

static void
set_overflow(layout_object_t *object, lxb_css_overflow_x_type_t overflow_x,
             lxb_css_overflow_y_type_t overflow_y)
{
    lxb_style_computed_t *style =
        (lxb_style_computed_t *) layout_object_style(object);
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "overflow style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);

    check(non_inherited != NULL, "overflow style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->overflow_x = overflow_x;
        non_inherited->overflow_y = overflow_y;
    }
}

static void
set_visibility(layout_object_t *object, lxb_css_visibility_type_t visibility)
{
    lxb_style_computed_t *style =
        (lxb_style_computed_t *) layout_object_style(object);
    lxb_style_computed_inherited_t *inherited;

    if (style == NULL) {
        check(0, "visibility style exists");
        return;
    }

    inherited = lxb_style_computed_inherited_mutable(style);

    check(inherited != NULL, "style inherited group is mutable");
    if (inherited != NULL) {
        inherited->visibility = visibility;
    }
}

static void
set_z_index(layout_object_t *object, int z_index)
{
    lxb_style_computed_t *style =
        (lxb_style_computed_t *) layout_object_style(object);
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "z-index style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);

    check(non_inherited != NULL, "z-index style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->z_index_auto = false;
        non_inherited->z_index = z_index;
    }
}

static void
set_opacity(layout_object_t *object, double opacity)
{
    lxb_style_computed_t *style =
        (lxb_style_computed_t *) layout_object_style(object);
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "opacity style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);

    check(non_inherited != NULL, "opacity style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->opacity = opacity;
    }
}

static void
set_style_display(lxb_style_computed_t *style,
                  lxb_css_display_type_t display_box,
                  lxb_css_display_type_t display_outside,
                  lxb_css_display_type_t display_inside)
{
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "display style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);
    check(non_inherited != NULL, "display style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->display_box = display_box;
        non_inherited->display_outside = display_outside;
        non_inherited->display_inside = display_inside;
    }
}

static void
set_style_position(lxb_style_computed_t *style,
                   lxb_css_position_type_t position)
{
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "position style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);
    check(non_inherited != NULL, "position style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->position = position;
    }
}

static void
set_style_float(lxb_style_computed_t *style,
                lxb_css_float_type_t float_type)
{
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "float style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);
    check(non_inherited != NULL, "float style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->floatp = float_type;
    }
}

static void
set_style_clear(lxb_style_computed_t *style,
                lxb_css_clear_type_t clear_type)
{
    lxb_style_computed_non_inherited_t *non_inherited;

    if (style == NULL) {
        check(0, "clear style exists");
        return;
    }

    non_inherited = lxb_style_computed_non_inherited_mutable(style);
    check(non_inherited != NULL, "clear style group is mutable");
    if (non_inherited != NULL) {
        non_inherited->clear = clear_type;
    }
}

static void
set_style_inset(lxb_style_computed_t *style, size_t edge, double px)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "inset style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "inset style group is mutable");
    if (box != NULL) {
        box->inset[edge].type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->inset[edge].u.length.num = px;
        box->inset[edge].u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
    }
}

static void
set_style_inset_percent(lxb_style_computed_t *style, size_t edge,
                        double percent)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "percentage inset style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "percentage inset style group is mutable");
    if (box != NULL) {
        box->inset[edge].type = LXB_STYLE_COMPUTED_VALUE_PERCENTAGE;
        box->inset[edge].u.number = percent;
    }
}

static void
set_style_size(lxb_style_computed_t *style, double width, double height)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "size style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "size style group is mutable");
    if (box != NULL) {
        box->width.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->width.u.length.num = width;
        box->width.u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
        box->height.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->height.u.length.num = height;
        box->height.u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
    }
}

static void
set_style_width(lxb_style_computed_t *style, double width)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "width style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "width style group is mutable");
    if (box != NULL) {
        box->width.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->width.u.length.num = width;
        box->width.u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
    }
}

static void
set_style_width_percent(lxb_style_computed_t *style, double percent)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "percentage width style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "percentage width style group is mutable");
    if (box != NULL) {
        box->width.type = LXB_STYLE_COMPUTED_VALUE_PERCENTAGE;
        box->width.u.number = percent;
    }
}

static void
set_style_min_width(lxb_style_computed_t *style, double width)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "min-width style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "min-width style group is mutable");
    if (box != NULL) {
        box->min_width.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->min_width.u.length.num = width;
        box->min_width.u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
    }
}

static void
set_style_max_width(lxb_style_computed_t *style, double width)
{
    lxb_style_computed_box_t *box;

    if (style == NULL) {
        check(0, "max-width style exists");
        return;
    }

    box = lxb_style_computed_box_mutable(style);
    check(box != NULL, "max-width style group is mutable");
    if (box != NULL) {
        box->max_width.type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
        box->max_width.u.length.num = width;
        box->max_width.u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
    }
}

static void
set_style_edge(lxb_style_computed_value_t *value, double px)
{
    if (value == NULL) {
        check(0, "edge style value exists");
        return;
    }

    value->type = LXB_STYLE_COMPUTED_VALUE_LENGTH;
    value->u.length.num = px;
    value->u.length.unit = (lxb_css_unit_t) LXB_CSS_UNIT_PX;
}

static void
set_style_edge_percent(lxb_style_computed_value_t *value, double percent)
{
    if (value == NULL) {
        check(0, "percentage edge style value exists");
        return;
    }

    value->type = LXB_STYLE_COMPUTED_VALUE_PERCENTAGE;
    value->u.number = percent;
}

static void
set_style_margin(lxb_style_computed_t *style, double top, double right,
                 double bottom, double left)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "margin style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "margin style group is mutable");
    if (surround != NULL) {
        set_style_edge(&surround->margin[LXB_STYLE_EDGE_TOP], top);
        set_style_edge(&surround->margin[LXB_STYLE_EDGE_RIGHT], right);
        set_style_edge(&surround->margin[LXB_STYLE_EDGE_BOTTOM], bottom);
        set_style_edge(&surround->margin[LXB_STYLE_EDGE_LEFT], left);
    }
}

static void
set_style_margin_percent(lxb_style_computed_t *style, double top,
                         double right, double bottom, double left)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "percentage margin style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "percentage margin style group is mutable");
    if (surround != NULL) {
        set_style_edge_percent(&surround->margin[LXB_STYLE_EDGE_TOP], top);
        set_style_edge_percent(&surround->margin[LXB_STYLE_EDGE_RIGHT], right);
        set_style_edge_percent(&surround->margin[LXB_STYLE_EDGE_BOTTOM],
                               bottom);
        set_style_edge_percent(&surround->margin[LXB_STYLE_EDGE_LEFT], left);
    }
}

static void
set_style_margin_auto(lxb_style_computed_t *style, size_t edge)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "auto margin style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "auto margin style group is mutable");
    if (surround != NULL) {
        surround->margin[edge].type = LXB_STYLE_COMPUTED_VALUE_AUTO;
    }
}

static void
set_style_padding(lxb_style_computed_t *style, double top, double right,
                  double bottom, double left)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "padding style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "padding style group is mutable");
    if (surround != NULL) {
        set_style_edge(&surround->padding[LXB_STYLE_EDGE_TOP], top);
        set_style_edge(&surround->padding[LXB_STYLE_EDGE_RIGHT], right);
        set_style_edge(&surround->padding[LXB_STYLE_EDGE_BOTTOM], bottom);
        set_style_edge(&surround->padding[LXB_STYLE_EDGE_LEFT], left);
    }
}

static void
set_style_padding_percent(lxb_style_computed_t *style, double top,
                          double right, double bottom, double left)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "percentage padding style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "percentage padding style group is mutable");
    if (surround != NULL) {
        set_style_edge_percent(&surround->padding[LXB_STYLE_EDGE_TOP], top);
        set_style_edge_percent(&surround->padding[LXB_STYLE_EDGE_RIGHT],
                               right);
        set_style_edge_percent(&surround->padding[LXB_STYLE_EDGE_BOTTOM],
                               bottom);
        set_style_edge_percent(&surround->padding[LXB_STYLE_EDGE_LEFT], left);
    }
}

static void
set_style_border(lxb_style_computed_t *style, double top, double right,
                 double bottom, double left)
{
    lxb_style_computed_surround_t *surround;

    if (style == NULL) {
        check(0, "border style exists");
        return;
    }

    surround = lxb_style_computed_surround_mutable(style);
    check(surround != NULL, "border style group is mutable");
    if (surround != NULL) {
        surround->border[LXB_STYLE_EDGE_TOP].style = LXB_CSS_BORDER_SOLID;
        set_style_edge(&surround->border[LXB_STYLE_EDGE_TOP].width, top);
        surround->border[LXB_STYLE_EDGE_RIGHT].style = LXB_CSS_BORDER_SOLID;
        set_style_edge(&surround->border[LXB_STYLE_EDGE_RIGHT].width, right);
        surround->border[LXB_STYLE_EDGE_BOTTOM].style = LXB_CSS_BORDER_SOLID;
        set_style_edge(&surround->border[LXB_STYLE_EDGE_BOTTOM].width, bottom);
        surround->border[LXB_STYLE_EDGE_LEFT].style = LXB_CSS_BORDER_SOLID;
        set_style_edge(&surround->border[LXB_STYLE_EDGE_LEFT].width, left);
    }
}

static void
init_element(lxb_dom_element_t *element, lxb_style_computed_t *style)
{
    memset(element, 0, sizeof(*element));
    element->node.type = LXB_DOM_NODE_TYPE_ELEMENT;
    element->computed_style = style;
}

static void
append_dom_child(lxb_dom_node_t *parent, lxb_dom_node_t *child)
{
    child->parent = parent;
    child->prev = parent->last_child;
    child->next = NULL;

    if (parent->last_child != NULL) {
        parent->last_child->next = child;
    }
    else {
        parent->first_child = child;
    }

    parent->last_child = child;
}

static layout_oof_positioned_t *
find_oof_for_object(layout_result_t *result, layout_object_t *object)
{
    size_t count;

    if (result == NULL || object == NULL) {
        return NULL;
    }

    count = layout_result_out_of_flow_positioned_count(result);
    for (size_t i = 0; i < count; i++) {
        layout_oof_positioned_t *oof =
            layout_result_out_of_flow_positioned_at(result, i);
        if (layout_oof_positioned_object(oof) == object) {
            return oof;
        }
    }

    return NULL;
}

static layout_fragment_t *
make_fragment(layout_result_t *result, layout_object_t *object, double width,
              double height)
{
    layout_fragment_init_t init;

    memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;

    return layout_fragment_create(result, &init);
}

static layout_fragment_t *
make_fragment_with_stacking_order(layout_result_t *result,
                                  layout_object_t *object,
                                  double width, double height,
                                  int stacking_order)
{
    layout_fragment_init_t init;

    memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    init.stacking_order = stacking_order;

    return layout_fragment_create(result, &init);
}

static layout_fragment_t *
make_fragment_with_flags(layout_result_t *result, layout_object_t *object,
                         double width, double height, unsigned flags)
{
    layout_fragment_init_t init;

    memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    init.flags = flags;

    return layout_fragment_create(result, &init);
}

static layout_fragment_t *
make_fragment_with_box_type(layout_result_t *result, layout_object_t *object,
                            double width, double height,
                            layout_fragment_box_type_t box_type)
{
    layout_fragment_init_t init;

    memset(&init, 0, sizeof(init));
    init.object = object;
    init.size.width = width;
    init.size.height = height;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = box_type;

    return layout_fragment_create(result, &init);
}

static void
node_style_reference_smoke(layout_t *layout)
{
    lxb_dom_element_t element;
    lxb_style_computed_t *style = make_style();
    layout_object_t *object;

    check(style != NULL, "computed style allocates");
    if (style == NULL) {
        return;
    }

    memset(&element, 0, sizeof(element));
    element.node.type = LXB_DOM_NODE_TYPE_ELEMENT;
    element.computed_style = style;

    check(layout_dom_node_ensure_layout_object(
              layout, lxb_dom_interface_node(&element), &object)
              == LXB_STATUS_OK,
          "layout object ensure for DOM node succeeds");

    check(object != NULL, "layout object binds DOM node computed style");
    check(layout_object_node(object) == lxb_dom_interface_node(&element),
          "non-anonymous layout object exposes node");
    check(!layout_object_is_anonymous(object),
          "DOM-backed layout object is not anonymous");
    check(layout_object_style(object) == style,
          "layout object references element computed style");

    lxb_style_computed_unref(style);
}

static void
layout_object_style_derived_bits_smoke(layout_t *layout)
{
    lxb_style_computed_t *static_style = make_style();
    lxb_style_computed_t *relative_style = make_style();
    lxb_style_computed_t *second_static_style = make_style();
    layout_object_t *object;

    check(static_style != NULL && relative_style != NULL
              && second_static_style != NULL,
          "style derived bit smoke styles allocate");
    if (static_style == NULL || relative_style == NULL
        || second_static_style == NULL) {
        goto done;
    }

    set_style_position(relative_style, LXB_CSS_POSITION_RELATIVE);

    object = layout_object_create_anonymous(layout, static_style);
    check(object != NULL, "style derived bit object allocates");
    if (object == NULL) {
        goto done;
    }

    check(!layout_object_can_have_children(object),
          "base anonymous object does not own children");
    check(!layout_object_can_contain_absolute_position(object),
          "static object is not an absolute containing block");
    check(layout_object_set_style(object, relative_style) == LXB_STATUS_OK,
          "style update to positioned succeeds");
    check(layout_object_can_contain_absolute_position(object),
          "relative style derives absolute containing block bit");
    check(layout_object_self_needs_full_layout(object),
          "style update marks object dirty");

    layout_object_clear_needs_layout(object);
    check(layout_object_set_style(object, second_static_style)
              == LXB_STATUS_OK,
          "style update back to static succeeds");
    check(!layout_object_can_contain_absolute_position(object),
          "static style clears derived absolute containing block bit");
    check(layout_object_self_needs_full_layout(object),
          "second style update marks object dirty");

done:
    if (static_style != NULL) {
        lxb_style_computed_unref(static_style);
    }
    if (relative_style != NULL) {
        lxb_style_computed_unref(relative_style);
    }
    if (second_static_style != NULL) {
        lxb_style_computed_unref(second_static_style);
    }
}

static void
layout_object_identity_smoke(layout_t *layout)
{
    layout_object_t *first = make_object(layout);
    layout_object_t *second = make_object(layout);
    lxb_dom_element_t element;
    lxb_style_computed_t *style = make_style();
    layout_object_t *dom_first;
    layout_object_t *dom_second;
    uint64_t first_id = layout_object_id(first);
    uint64_t second_id = layout_object_id(second);
    uint64_t dom_id;

    check(first != NULL && second != NULL,
          "layout object id smoke objects allocate");
    check(first_id != 0 && second_id != 0 && first_id != second_id,
          "layout object ids are nonzero and unique");

    check(style != NULL, "layout object id DOM style allocates");
    if (style != NULL) {
        init_element(&element, style);
        check(layout_dom_node_ensure_layout_object(
                  layout, lxb_dom_interface_node(&element), &dom_first)
                  == LXB_STATUS_OK,
              "DOM-backed first layout object ensures");
        dom_id = layout_object_id(dom_first);
        layout_clean(layout);
        element.computed_style = style;
        check(layout_dom_node_ensure_layout_object(
                  layout, lxb_dom_interface_node(&element), &dom_second)
                  == LXB_STATUS_OK,
              "DOM-backed second layout object ensures");
        check(dom_first != NULL && dom_second != NULL,
              "DOM-backed layout objects allocate across clean");
        check(dom_id != 0 && layout_object_id(dom_second) == dom_id,
              "DOM-backed layout object id is stable for the same node");
    }

    layout_clean(layout);

    first = make_object(layout);
    check(first != NULL, "layout object id smoke object allocates after clean");
    check(layout_object_id(first) != first_id
              && layout_object_id(first) != second_id,
          "anonymous layout object ids are not reused after layout_clean");

    if (style != NULL) {
        lxb_style_computed_unref(style);
    }
}

static void
layout_tree_builder_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *first_style = make_style();
    lxb_style_computed_t *contents_style = make_style();
    lxb_style_computed_t *flattened_style = make_style();
    lxb_style_computed_t *none_style = make_style();
    lxb_style_computed_t *hidden_child_style = make_style();
    lxb_style_computed_t *positioned_style = make_style();
    lxb_style_computed_t *inline_style = make_style();
    lxb_style_computed_t *inline_child_style = make_style();
    lxb_style_computed_t *transparent_root_style = make_style();
    lxb_style_computed_t *transparent_child_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t first;
    lxb_dom_element_t contents;
    lxb_dom_element_t flattened;
    lxb_dom_element_t none;
    lxb_dom_element_t hidden_child;
    lxb_dom_element_t positioned;
    lxb_dom_element_t inline_node;
    lxb_dom_element_t inline_child;
    lxb_dom_element_t transparent_root;
    lxb_dom_element_t transparent_child;
    layout_tree_t *tree = NULL;
    layout_object_t *root_object;
    layout_object_t *first_object;
    layout_object_t *flattened_object;
    layout_object_t *positioned_object;
    layout_object_t *inline_object;
    layout_object_t *inline_child_object;
    layout_object_t *anonymous_block_object;
    layout_object_t *detached_object;
    layout_object_child_list_t *root_children;
    layout_object_child_list_t *anonymous_children;

    check(root_style != NULL && first_style != NULL && contents_style != NULL
              && flattened_style != NULL && none_style != NULL
              && hidden_child_style != NULL && positioned_style != NULL
              && inline_style != NULL && inline_child_style != NULL
              && transparent_root_style != NULL
              && transparent_child_style != NULL,
          "layout tree smoke styles allocate");
    if (root_style == NULL || first_style == NULL || contents_style == NULL
        || flattened_style == NULL || none_style == NULL
        || hidden_child_style == NULL || positioned_style == NULL
        || inline_style == NULL || inline_child_style == NULL
        || transparent_root_style == NULL || transparent_child_style == NULL) {
        goto done;
    }

    set_style_display(first_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(contents_style, LXB_CSS_DISPLAY_CONTENTS,
                      LXB_CSS_DISPLAY_INLINE, LXB_CSS_DISPLAY_FLOW);
    set_style_display(flattened_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(transparent_root_style, LXB_CSS_DISPLAY_CONTENTS,
                      LXB_CSS_DISPLAY_INLINE, LXB_CSS_DISPLAY_FLOW);
    set_style_display(inline_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_INLINE, LXB_CSS_DISPLAY_FLOW);
    set_style_display(inline_child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(positioned_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(none_style, LXB_CSS_DISPLAY_NONE,
                      LXB_CSS_DISPLAY_INLINE, LXB_CSS_DISPLAY_FLOW);
    set_style_position(positioned_style, LXB_CSS_POSITION_RELATIVE);

    init_element(&root, root_style);
    init_element(&first, first_style);
    init_element(&contents, contents_style);
    init_element(&flattened, flattened_style);
    init_element(&none, none_style);
    init_element(&hidden_child, hidden_child_style);
    init_element(&positioned, positioned_style);
    init_element(&inline_node, inline_style);
    init_element(&inline_child, inline_child_style);
    init_element(&transparent_root, transparent_root_style);
    init_element(&transparent_child, transparent_child_style);

    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&first));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&contents));
    append_dom_child(lxb_dom_interface_node(&contents),
                     lxb_dom_interface_node(&flattened));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&none));
    append_dom_child(lxb_dom_interface_node(&none),
                     lxb_dom_interface_node(&hidden_child));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&positioned));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&inline_node));
    append_dom_child(lxb_dom_interface_node(&inline_node),
                     lxb_dom_interface_node(&inline_child));

    tree = layout_tree_create(layout);
    check(tree != NULL, "layout tree create returns object");
    if (tree == NULL) {
        goto done;
    }

    check(layout_tree_layout(tree) == layout,
          "layout tree references owning layout context");
    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "layout tree builds from DOM root");

    root_object = layout_tree_root_object(tree);
    first_object = layout_tree_object_for_node(tree,
                                              lxb_dom_interface_node(&first));
    flattened_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&flattened));
    positioned_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&positioned));
    inline_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&inline_node));
    inline_child_object =
        layout_tree_object_for_node(tree,
                                    lxb_dom_interface_node(&inline_child));
    root_children = layout_tree_children(tree, root_object);

    check(root_object == layout_tree_object_for_node(
                             tree, lxb_dom_interface_node(&root)),
          "tree maps root DOM node to root layout object");
    check(root_object != NULL && layout_object_style(root_object) == root_style,
          "tree root references DOM computed style");
    check(layout_object_can_have_children(root_object),
          "tree root is created as block-capable layout object");
    check(layout_object_can_contain_absolute_position(root_object)
              && layout_object_can_contain_fixed_position(root_object),
          "tree root initializes viewport containing block bits");
    check(layout_object_set_style(root_object, first_style) == LXB_STATUS_OK,
          "tree root style update succeeds");
    check(layout_object_can_contain_absolute_position(root_object)
              && layout_object_can_contain_fixed_position(root_object),
          "tree root keeps viewport containing block bits after style update");
    check(layout_tree_object_for_node(tree, lxb_dom_interface_node(&contents))
              == NULL,
          "display:contents element does not create layout object");
    check(layout_tree_object_for_node(tree, lxb_dom_interface_node(&none))
              == NULL,
          "display:none element does not create layout object");
    check(layout_tree_object_for_node(tree,
                                      lxb_dom_interface_node(&hidden_child))
              == NULL,
          "display:none subtree is skipped");

    check(root_children != NULL, "root object has external child list record");
    check(layout_object_slow_first_child(root_children) == first_object,
          "first DOM child is first layout child");
    check(layout_object_next_sibling(first_object) == flattened_object,
          "display:contents child is flattened into layout sibling list");
    check(layout_object_parent(flattened_object) == root_object,
          "flattened child uses nearest layout parent");
    check(layout_object_next_sibling(flattened_object) == positioned_object,
          "display:none subtree is absent from sibling chain");
    check(layout_object_previous_sibling(positioned_object)
              == flattened_object,
          "layout builder preserves previous sibling links");
    anonymous_block_object = layout_object_next_sibling(positioned_object);
    anonymous_children = layout_tree_children(tree, anonymous_block_object);
    check(anonymous_block_object != NULL
              && layout_object_is_anonymous(anonymous_block_object),
          "inline object is wrapped in anonymous block");
    check(layout_object_slow_last_child(root_children)
              == anonymous_block_object,
          "anonymous block is last root child");
    check(anonymous_children != NULL,
          "anonymous block owns children");
    check(layout_object_slow_first_child(anonymous_children) == inline_object,
          "anonymous block contains inline object");
    check(layout_object_next_sibling(inline_object) == inline_child_object,
          "inline child is inserted after inline object in anonymous block");
    check(layout_tree_children(tree, flattened_object) != NULL,
          "block layout object owns child list through block extension");
    check(layout_object_can_have_children(flattened_object),
          "builder-created visible elements are block-capable objects");
    check(!layout_object_can_have_children(inline_object),
          "inline layout object does not own children yet");
    check(layout_object_parent(inline_child_object) == anonymous_block_object,
          "inline descendant uses anonymous block parent");
    check(layout_object_can_contain_absolute_position(positioned_object),
          "positioned object initializes absolute containing block bit");
    check(!layout_object_can_contain_fixed_position(positioned_object),
          "non-root positioned object does not become fixed containing block");
    detached_object = layout_object_create_anonymous(layout, root_style);
    check(detached_object != NULL, "detached traversal smoke object allocates");
    check(layout_tree_object_next_in_preorder(tree, root_object, NULL)
              == first_object,
          "tree preorder starts at first child");
    check(layout_tree_object_next_in_preorder(tree, first_object, NULL)
              == flattened_object,
          "tree preorder walks to next sibling after leaf");
    check(layout_tree_object_next_in_preorder(tree, positioned_object, NULL)
              == anonymous_block_object,
          "tree preorder reaches anonymous wrapper sibling");
    check(layout_tree_object_next_in_preorder(tree, anonymous_block_object,
                                             NULL)
              == inline_object,
          "tree preorder enters external child list through tree context");
    check(layout_tree_object_next_in_preorder(tree, inline_object, NULL)
              == inline_child_object,
          "tree preorder visits anonymous block descendants");
    check(layout_tree_object_next_in_preorder(tree, inline_child_object,
                                             NULL) == NULL,
          "tree preorder ends after last descendant");
    check(layout_tree_object_next_in_preorder_after_children(
              tree, anonymous_block_object, NULL) == NULL,
          "tree preorder after children skips anonymous subtree");
    check(layout_tree_object_next_in_preorder_after_children(
              tree, first_object, root_object) == flattened_object,
          "tree preorder after children stays inside requested subtree");
    check(layout_tree_object_next_in_preorder_after_children(
              tree, anonymous_block_object, root_object) == NULL,
          "tree preorder after children stops at stay-within boundary");
    check(layout_tree_object_previous_in_preorder(tree, root_object, NULL)
              == NULL,
          "tree reverse preorder has no object before root");
    check(layout_tree_object_previous_in_preorder(tree, flattened_object,
                                                 NULL)
              == first_object,
          "tree reverse preorder returns previous sibling leaf");
    check(layout_tree_object_previous_in_preorder(tree, inline_child_object,
                                                 NULL)
              == inline_object,
          "tree reverse preorder returns previous sibling in wrapper");
    check(layout_tree_object_previous_in_preorder(tree, inline_object, NULL)
              == anonymous_block_object,
          "tree reverse preorder returns parent for first child");
    check(layout_tree_object_previous_in_preorder(tree, inline_object,
                                                 anonymous_block_object)
              == anonymous_block_object,
          "tree reverse preorder may return stay-within ancestor");
    check(layout_tree_object_previous_in_preorder(tree,
                                                 anonymous_block_object,
                                                 anonymous_block_object)
              == NULL,
          "tree reverse preorder stops on stay-within object");
    check(layout_tree_object_previous_in_postorder(tree, root_object, NULL)
              == anonymous_block_object,
          "tree reverse postorder enters last child first");
    check(layout_tree_object_previous_in_postorder(tree,
                                                  anonymous_block_object,
                                                  root_object)
              == inline_child_object,
          "tree reverse postorder enters last descendant through tree context");
    check(layout_tree_object_previous_in_postorder(tree, inline_child_object,
                                                  root_object)
              == inline_object,
          "tree reverse postorder falls back to previous sibling");
    check(layout_tree_object_previous_in_postorder(tree, first_object,
                                                  root_object) == NULL,
          "tree reverse postorder stops at stay-within subtree boundary");
    check(layout_tree_object_next_in_preorder(tree, detached_object, NULL)
              == NULL,
          "tree preorder rejects object outside tree records");
    check(layout_tree_object_next_in_preorder(tree, first_object,
                                             detached_object) == NULL,
          "tree preorder rejects stay-within outside tree records");

    layout_tree_clean(tree);
    append_dom_child(lxb_dom_interface_node(&transparent_root),
                     lxb_dom_interface_node(&transparent_child));
    check(layout_tree_build(tree, lxb_dom_interface_node(&transparent_root))
              == LXB_STATUS_OK,
          "display:contents root with one layout child builds");
    check(layout_tree_root_object(tree)
              == layout_tree_object_for_node(
                  tree, lxb_dom_interface_node(&transparent_child)),
          "display:contents root is transparent to its single layout child");
    check(layout_tree_object_for_node(
              tree, lxb_dom_interface_node(&transparent_root)) == NULL,
          "display:contents root does not create a layout object");

done:
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (first_style != NULL) {
        lxb_style_computed_unref(first_style);
    }
    if (contents_style != NULL) {
        lxb_style_computed_unref(contents_style);
    }
    if (flattened_style != NULL) {
        lxb_style_computed_unref(flattened_style);
    }
    if (none_style != NULL) {
        lxb_style_computed_unref(none_style);
    }
    if (hidden_child_style != NULL) {
        lxb_style_computed_unref(hidden_child_style);
    }
    if (positioned_style != NULL) {
        lxb_style_computed_unref(positioned_style);
    }
    if (inline_style != NULL) {
        lxb_style_computed_unref(inline_style);
    }
    if (inline_child_style != NULL) {
        lxb_style_computed_unref(inline_child_style);
    }
    if (transparent_root_style != NULL) {
        lxb_style_computed_unref(transparent_root_style);
    }
    if (transparent_child_style != NULL) {
        lxb_style_computed_unref(transparent_child_style);
    }
}

static void
layout_tree_fragment_builder_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *first_style = make_style();
    lxb_style_computed_t *absolute_style = make_style();
    lxb_style_computed_t *nested_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t first;
    lxb_dom_element_t absolute;
    lxb_dom_element_t nested;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_object_t *root_object = NULL;
    layout_object_t *first_object = NULL;
    layout_object_t *absolute_object = NULL;
    layout_object_t *nested_object = NULL;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *first_fragment = NULL;
    layout_fragment_t *absolute_fragment = NULL;
    layout_fragment_t *nested_fragment = NULL;
    layout_fragment_t *hit = NULL;
    layout_oof_positioned_t *oof = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && first_style != NULL
              && absolute_style != NULL && nested_style != NULL,
          "layout tree fragment builder styles allocate");
    if (root_style == NULL || first_style == NULL
        || absolute_style == NULL || nested_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(first_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(absolute_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(nested_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_position(absolute_style, LXB_CSS_POSITION_ABSOLUTE);
    set_style_size(root_style, 300.0, 200.0);
    set_style_size(first_style, 120.0, 40.0);
    set_style_size(nested_style, 50.0, 20.0);

    init_element(&root, root_style);
    init_element(&first, first_style);
    init_element(&absolute, absolute_style);
    init_element(&nested, nested_style);

    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&first));
    append_dom_child(lxb_dom_interface_node(&first),
                     lxb_dom_interface_node(&nested));
    append_dom_child(lxb_dom_interface_node(&first),
                     lxb_dom_interface_node(&absolute));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "layout tree fragment builder contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "layout tree fragment builder builds layout tree");
    root_object = layout_tree_root_object(tree);
    first_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&first));
    absolute_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&absolute));
    nested_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&nested));

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 300.0;
    builder.viewport_size.height = 200.0;
    builder.default_object_size.width = 0.0;
    builder.default_object_size.height = 10.0;
    builder.default_block_gap = 4.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "layout tree fragment builder creates physical result tree");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL
              && layout_fragment_object(root_fragment) == root_object,
          "fragment builder stores root physical fragment");
    check(!layout_result_needs_layout(result),
          "fragment builder clears dirty bits for committed result");
    check(layout_fragment_child_count(root_fragment) == 1,
          "absolute child is excluded from normal flow fragment links");
    first_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            root_fragment));
    check(first_fragment != NULL
              && layout_fragment_object(first_fragment) == first_object,
          "fragment builder links normal-flow child fragment");
    check(layout_fragment_child_count(first_fragment) == 1,
          "fragment builder links nested normal-flow child");
    nested_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            first_fragment));
    check(nested_fragment != NULL
              && layout_fragment_object(nested_fragment) == nested_object,
          "fragment builder creates nested child fragment");
    check(layout_result_fragment_link_offset(result, first_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "fragment builder exposes nested fragment offset");
    check(offset.x == 0.0 && offset.y == 0.0,
          "first nested fragment starts at block-start");
    check(layout_result_out_of_flow_positioned_count(result) == 1,
          "fragment builder registers absolute child as out-of-flow");
    oof = layout_result_out_of_flow_positioned_at(result, 0);
    absolute_fragment = layout_oof_positioned_fragment(oof);
    check(layout_oof_positioned_object(oof) == absolute_object,
          "fragment builder OOF registry stores absolute object");
    check(layout_oof_positioned_containing_block(oof) == root_object,
          "fragment builder OOF registry stores containing block");
    check(layout_oof_positioned_containing_fragment(oof) == root_fragment,
          "fragment builder OOF registry stores containing fragment");
    offset = layout_oof_positioned_offset(oof);
    check(offset.x == 0.0 && offset.y == 24.0,
          "fragment builder stores OOF offset relative to containing block");
    check(layout_oof_positioned_placement(oof)
              == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW,
          "fragment builder OOF registry exposes out-of-flow placement");
    check(absolute_fragment != NULL
              && layout_fragment_object(absolute_fragment) == absolute_object,
          "fragment builder stores absolute physical fragment");
    check(layout_fragment_role(absolute_fragment)
              == LAYOUT_FRAGMENT_ROLE_OUT_OF_FLOW,
          "absolute physical fragment exposes out-of-flow role");
    check(layout_result_fragment_size(result, absolute_fragment, &size)
              == LXB_STATUS_OK,
          "fragment builder exposes absolute fragment size");
    check(size.width == 300.0 && size.height == 10.0,
          "absolute default size uses containing block constraints");
    check(layout_hit_test(result, NULL, make_point(10.0, 10.0), &hit)
              == LXB_STATUS_OK,
          "fragment builder result supports hit test");
    check(hit == nested_fragment,
          "fragment builder hit test reaches deepest normal-flow fragment");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (first_style != NULL) {
        lxb_style_computed_unref(first_style);
    }
    if (absolute_style != NULL) {
        lxb_style_computed_unref(absolute_style);
    }
    if (nested_style != NULL) {
        lxb_style_computed_unref(nested_style);
    }
}

static void
layout_block_reflow_auto_height_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *child_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t child;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *child_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;
    layout_box_edges_t edges;
    layout_rect_t content_rect;

    check(root_style != NULL && child_style != NULL,
          "block reflow styles allocate");
    if (root_style == NULL || child_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_width(root_style, 200.0);
    set_style_size(child_style, 80.0, 30.0);
    set_style_border(root_style, 2.0, 4.0, 6.0, 8.0);
    set_style_padding(root_style, 6.0, 8.0, 10.0, 12.0);
    set_style_margin(child_style, 5.0, 7.0, 9.0, 11.0);

    init_element(&root, root_style);
    init_element(&child, child_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&child));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "block reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "block reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 300.0;
    builder.viewport_size.height = 200.0;
    builder.default_object_size.width = 0.0;
    builder.default_object_size.height = 0.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "block reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    child_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            root_fragment));
    check(root_fragment != NULL && child_fragment != NULL,
          "block reflow creates root and child fragments");
    check(layout_result_fragment_size(result, root_fragment, &size)
              == LXB_STATUS_OK,
          "block reflow exposes root auto size");
    check(size.width == 232.0 && size.height == 68.0,
          "root auto border-box includes content width, border, padding, child margin");
    edges = layout_fragment_border(root_fragment);
    check(near_double(edges.top, 2.0) && near_double(edges.right, 4.0)
              && near_double(edges.bottom, 6.0)
              && near_double(edges.left, 8.0)
              && layout_fragment_border_sides(root_fragment)
                     == LAYOUT_BOX_SIDE_ALL,
          "root fragment copies used physical border facts");
    edges = layout_fragment_padding(root_fragment);
    check(near_double(edges.top, 6.0) && near_double(edges.right, 8.0)
              && near_double(edges.bottom, 10.0)
              && near_double(edges.left, 12.0),
          "root fragment copies used physical padding facts");
    content_rect = layout_fragment_content_rect(root_fragment);
    check(near_double(content_rect.x, 20.0)
              && near_double(content_rect.y, 8.0)
              && near_double(content_rect.width, 200.0)
              && near_double(content_rect.height, 44.0),
          "root fragment exposes content rect from used border and padding");
    check(layout_result_fragment_size(result, child_fragment, &size)
              == LXB_STATUS_OK,
          "block reflow exposes child size");
    check(size.width == 80.0 && size.height == 30.0,
          "child explicit content-box size has zero edges");
    edges = layout_fragment_margin(child_fragment);
    check(near_double(edges.top, 5.0) && near_double(edges.right, 7.0)
              && near_double(edges.bottom, 9.0)
              && near_double(edges.left, 11.0),
          "child fragment copies used physical margin facts");
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "block reflow exposes child placement");
    check(offset.x == 31.0 && offset.y == 13.0,
          "child offset includes parent border, padding, and child margin");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (child_style != NULL) {
        lxb_style_computed_unref(child_style);
    }
}

static void
layout_reflow_input_constraints_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *child_style = make_style();
    lxb_style_computed_t *absolute_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t child;
    lxb_dom_element_t absolute;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *child_fragment = NULL;
    layout_fragment_t *absolute_fragment = NULL;
    layout_oof_positioned_t *oof = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && child_style != NULL
              && absolute_style != NULL,
          "reflow input constraint styles allocate");
    if (root_style == NULL || child_style == NULL
        || absolute_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(absolute_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 100.0);
    set_style_padding_percent(root_style, 10.0, 5.0, 10.0, 5.0);
    set_style_margin_percent(child_style, 5.0, 0.0, 5.0, 0.0);
    set_style_width_percent(child_style, 80.0);
    set_style_min_width(child_style, 180.0);
    set_style_max_width(child_style, 150.0);
    set_style_position(absolute_style, LXB_CSS_POSITION_ABSOLUTE);

    init_element(&root, root_style);
    init_element(&child, child_style);
    init_element(&absolute, absolute_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&child));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&absolute));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "reflow input constraint contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "reflow input constraint builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 300.0;
    builder.viewport_size.height = 200.0;
    builder.default_object_size.width = 0.0;
    builder.default_object_size.height = 10.0;
    builder.default_block_gap = 10.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "reflow input constraint builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    child_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            root_fragment));
    check(root_fragment != NULL && child_fragment != NULL,
          "reflow input constraint creates root and child fragments");
    check(layout_result_fragment_size(result, root_fragment, &size)
              == LXB_STATUS_OK,
          "reflow input constraint exposes root size");
    check(size.width == 230.0 && size.height == 160.0,
          "percentage padding is resolved against containing inline size");
    check(layout_result_fragment_size(result, child_fragment, &size)
              == LXB_STATUS_OK,
          "reflow input constraint exposes child size");
    check(size.width == 180.0 && size.height == 10.0,
          "min-width greater than max-width clamps max to min");
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "reflow input constraint exposes child offset");
    check(offset.x == 15.0 && offset.y == 40.0,
          "percentage padding and margin affect normal-flow placement");
    check(layout_result_out_of_flow_positioned_count(result) == 1,
          "reflow input constraint registers absolute child");
    oof = layout_result_out_of_flow_positioned_at(result, 0);
    absolute_fragment = layout_oof_positioned_fragment(oof);
    check(layout_result_fragment_size(result, absolute_fragment, &size)
              == LXB_STATUS_OK,
          "reflow input constraint exposes absolute size");
    check(size.width == 230.0 && size.height == 10.0,
          "absolute default width uses containing block padding box");
    offset = layout_oof_positioned_offset(oof);
    check(offset.x == 15.0 && offset.y == 70.0,
          "absolute static offset uses parent hypothetical flow position");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (child_style != NULL) {
        lxb_style_computed_unref(child_style);
    }
    if (absolute_style != NULL) {
        lxb_style_computed_unref(absolute_style);
    }
}

static void
positioned_inset_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *wrapper_style = make_style();
    lxb_style_computed_t *absolute_style = make_style();
    lxb_style_computed_t *fixed_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t wrapper;
    lxb_dom_element_t absolute;
    lxb_dom_element_t fixed;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_object_t *root_object = NULL;
    layout_object_t *absolute_object = NULL;
    layout_object_t *fixed_object = NULL;
    layout_fragment_t *root_fragment = NULL;
    layout_oof_positioned_t *absolute_oof = NULL;
    layout_oof_positioned_t *fixed_oof = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && wrapper_style != NULL
              && absolute_style != NULL && fixed_style != NULL,
          "positioned inset reflow styles allocate");
    if (root_style == NULL || wrapper_style == NULL
        || absolute_style == NULL || fixed_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(wrapper_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(absolute_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(fixed_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 400.0, 300.0);
    set_style_padding(root_style, 20.0, 30.0, 40.0, 50.0);
    set_style_size(wrapper_style, 100.0, 50.0);
    set_style_size(absolute_style, 70.0, 30.0);
    set_style_margin(absolute_style, 3.0, 4.0, 5.0, 6.0);
    set_style_position(absolute_style, LXB_CSS_POSITION_ABSOLUTE);
    set_style_inset_percent(absolute_style, LXB_STYLE_EDGE_LEFT, 20.0);
    set_style_inset_percent(absolute_style, LXB_STYLE_EDGE_TOP, 25.0);
    set_style_size(fixed_style, 40.0, 25.0);
    set_style_margin(fixed_style, 1.0, 7.0, 9.0, 2.0);
    set_style_position(fixed_style, LXB_CSS_POSITION_FIXED);
    set_style_inset(fixed_style, LXB_STYLE_EDGE_RIGHT, 20.0);
    set_style_inset(fixed_style, LXB_STYLE_EDGE_BOTTOM, 15.0);

    init_element(&root, root_style);
    init_element(&wrapper, wrapper_style);
    init_element(&absolute, absolute_style);
    init_element(&fixed, fixed_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&wrapper));
    append_dom_child(lxb_dom_interface_node(&wrapper),
                     lxb_dom_interface_node(&absolute));
    append_dom_child(lxb_dom_interface_node(&wrapper),
                     lxb_dom_interface_node(&fixed));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "positioned inset reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "positioned inset reflow builds layout tree");
    root_object = layout_tree_root_object(tree);
    absolute_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&absolute));
    fixed_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&fixed));

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 500.0;
    builder.viewport_size.height = 400.0;
    builder.default_object_size.width = 0.0;
    builder.default_object_size.height = 0.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "positioned inset reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && root_object != NULL
              && absolute_object != NULL && fixed_object != NULL,
          "positioned inset reflow resolves layout objects");
    check(layout_fragment_child_count(root_fragment) == 1,
          "positioned inset reflow keeps OOF children out of normal links");
    check(layout_result_out_of_flow_positioned_count(result) == 2,
          "positioned inset reflow registers absolute and fixed children");

    absolute_oof = find_oof_for_object(result, absolute_object);
    fixed_oof = find_oof_for_object(result, fixed_object);
    check(absolute_oof != NULL && fixed_oof != NULL,
          "positioned inset reflow finds OOF registry entries");

    check(layout_oof_positioned_containing_block(absolute_oof)
                  == root_object
              && layout_oof_positioned_containing_fragment(absolute_oof)
                     == root_fragment,
          "absolute inset resolves against root containing block fragment");
    offset = layout_oof_positioned_offset(absolute_oof);
    check(offset.x == 102.0 && offset.y == 93.0,
          "absolute inset offset uses containing block padding box");
    check(layout_result_fragment_size(
              result, layout_oof_positioned_fragment(absolute_oof), &size)
              == LXB_STATUS_OK,
          "absolute inset reflow exposes fragment size");
    check(size.width == 70.0 && size.height == 30.0,
          "absolute inset reflow keeps explicit size");

    check(layout_oof_positioned_is_fixed(fixed_oof),
          "fixed inset OOF entry records fixed positioning");
    check(layout_oof_positioned_containing_block(fixed_oof) == root_object,
          "fixed inset resolves against root fixed containing block");
    offset = layout_oof_positioned_offset(fixed_oof);
    check(offset.x == 413.0 && offset.y == 311.0,
          "fixed inset offset solves from right and bottom edges");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (wrapper_style != NULL) {
        lxb_style_computed_unref(wrapper_style);
    }
    if (absolute_style != NULL) {
        lxb_style_computed_unref(absolute_style);
    }
    if (fixed_style != NULL) {
        lxb_style_computed_unref(fixed_style);
    }
}

static void
relative_position_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *relative_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t relative;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_object_t *relative_object = NULL;
    layout_object_t *sibling_object = NULL;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *relative_fragment = NULL;
    layout_fragment_t *sibling_fragment = NULL;
    layout_point_t offset;
    layout_point_t local = make_point(0.0, 0.0);
    layout_rect_t local_rect;
    layout_rect_t root_rect;
    layout_fragment_t *hit = NULL;

    check(root_style != NULL && relative_style != NULL
              && sibling_style != NULL,
          "relative reflow styles allocate");
    if (root_style == NULL || relative_style == NULL
        || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(relative_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 120.0);
    set_style_size(relative_style, 40.0, 20.0);
    set_style_position(relative_style, LXB_CSS_POSITION_RELATIVE);
    set_style_inset(relative_style, LXB_STYLE_EDGE_LEFT, 15.0);
    set_style_inset(relative_style, LXB_STYLE_EDGE_TOP, 7.0);
    set_style_size(sibling_style, 50.0, 20.0);

    init_element(&root, root_style);
    init_element(&relative, relative_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&relative));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "relative reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "relative reflow builds layout tree");
    relative_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&relative));
    sibling_object =
        layout_tree_object_for_node(tree, lxb_dom_interface_node(&sibling));

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 120.0;
    builder.default_object_size.width = 0.0;
    builder.default_object_size.height = 0.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "relative reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && relative_object != NULL
              && sibling_object != NULL,
          "relative reflow resolves layout objects");
    check(layout_fragment_child_count(root_fragment) == 2,
          "relative reflow keeps positioned child in normal flow");
    check(layout_result_out_of_flow_positioned_count(result) == 0,
          "relative reflow does not register OOF geometry");

    relative_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    sibling_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 1));
    check(layout_fragment_object(relative_fragment) == relative_object
              && layout_fragment_object(sibling_fragment) == sibling_object,
          "relative reflow preserves normal child order");

    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "relative reflow exposes relative child normal slot");
    check(offset.x == 0.0 && offset.y == 0.0,
          "relative offset does not mutate link placement");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "relative reflow exposes following sibling slot");
    check(offset.x == 0.0 && offset.y == 20.0,
          "following sibling layout uses relative child's original slot");

    local_rect.x = 0.0;
    local_rect.y = 0.0;
    local_rect.width = 40.0;
    local_rect.height = 20.0;
    check(layout_result_fragment_rect_to_root(
              result, relative_fragment, local_rect,
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &root_rect)
              == LXB_STATUS_OK,
          "relative local rect maps to transformed root coordinates");
    check(root_rect.x == 15.0 && root_rect.y == 7.0
              && root_rect.width == 40.0 && root_rect.height == 20.0,
          "relative inset affects visual geometry only");

    check(layout_hit_test(result, root_fragment, make_point(20.0, 10.0),
                          &hit)
              == LXB_STATUS_OK,
          "relative visual point can be hit");
    check(hit == relative_fragment,
          "hit test uses relative visual offset");
    check(layout_hit_test(result, root_fragment, make_point(5.0, 5.0),
                          &hit)
              == LXB_STATUS_OK,
          "relative original-only point can be tested");
    check(hit == root_fragment,
          "relative original slot outside shifted visual rect does not hit");

    check(layout_result_root_point_to_object(
              result, relative_object, 0, make_point(25.0, 12.0),
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &local, NULL)
              == LXB_STATUS_OK,
          "relative root point maps through visual offset");
    check(local.x == 10.0 && local.y == 5.0,
          "relative inverse transform returns local coordinates");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (relative_style != NULL) {
        lxb_style_computed_unref(relative_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
block_margin_collapse_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *first_style = make_style();
    lxb_style_computed_t *second_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t first;
    lxb_dom_element_t second;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_point_t offset;

    check(root_style != NULL && first_style != NULL && second_style != NULL,
          "margin collapse reflow styles allocate");
    if (root_style == NULL || first_style == NULL || second_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(first_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(second_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 120.0);
    set_style_size(first_style, 40.0, 10.0);
    set_style_margin(first_style, 0.0, 0.0, 30.0, 0.0);
    set_style_size(second_style, 40.0, 10.0);
    set_style_margin(second_style, 20.0, 0.0, 0.0, 0.0);

    init_element(&root, root_style);
    init_element(&first, first_style);
    init_element(&second, second_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&first));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&second));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "margin collapse reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "margin collapse reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 120.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "margin collapse reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && layout_fragment_child_count(root_fragment) == 2,
          "margin collapse reflow creates sibling fragments");
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "margin collapse exposes first sibling offset");
    check(offset.x == 0.0 && offset.y == 0.0,
          "first block starts at the root content edge");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "margin collapse exposes second sibling offset");
    check(offset.x == 0.0 && offset.y == 40.0,
          "adjacent sibling margins collapse to the larger margin");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (first_style != NULL) {
        lxb_style_computed_unref(first_style);
    }
    if (second_style != NULL) {
        lxb_style_computed_unref(second_style);
    }
}

static void
empty_block_margin_collapse_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *empty_style = make_style();
    lxb_style_computed_t *second_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t empty;
    lxb_dom_element_t second;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_point_t offset;

    check(root_style != NULL && empty_style != NULL && second_style != NULL,
          "empty margin collapse reflow styles allocate");
    if (root_style == NULL || empty_style == NULL || second_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(empty_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(second_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 120.0);
    set_style_width(empty_style, 40.0);
    set_style_margin(empty_style, 10.0, 0.0, 30.0, 0.0);
    set_style_size(second_style, 40.0, 10.0);
    set_style_margin(second_style, 20.0, 0.0, 0.0, 0.0);

    init_element(&root, root_style);
    init_element(&empty, empty_style);
    init_element(&second, second_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&empty));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&second));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "empty margin collapse reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "empty margin collapse reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 120.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "empty margin collapse reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && layout_fragment_child_count(root_fragment) == 2,
          "empty margin collapse reflow creates sibling fragments");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "empty margin collapse exposes second sibling offset");
    check(offset.x == 0.0 && offset.y == 30.0,
          "empty block margins collapse before the next sibling margin");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (empty_style != NULL) {
        lxb_style_computed_unref(empty_style);
    }
    if (second_style != NULL) {
        lxb_style_computed_unref(second_style);
    }
}

static void
parent_child_margin_collapse_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *parent_style = make_style();
    lxb_style_computed_t *empty_style = make_style();
    lxb_style_computed_t *child_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t parent;
    lxb_dom_element_t empty;
    lxb_dom_element_t child;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *parent_fragment = NULL;
    layout_fragment_t *empty_fragment = NULL;
    layout_fragment_t *child_fragment = NULL;
    layout_fragment_t *sibling_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && parent_style != NULL && empty_style != NULL
              && child_style != NULL && sibling_style != NULL,
          "parent-child margin collapse styles allocate");
    if (root_style == NULL || parent_style == NULL || empty_style == NULL
        || child_style == NULL || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(parent_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(empty_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 140.0);
    set_style_width(parent_style, 100.0);
    set_style_margin(parent_style, 10.0, 0.0, 5.0, 0.0);
    set_style_width(empty_style, 40.0);
    set_style_margin(empty_style, 15.0, 0.0, 35.0, 0.0);
    set_style_size(child_style, 40.0, 10.0);
    set_style_margin(child_style, 30.0, 0.0, 40.0, 0.0);
    set_style_size(sibling_style, 40.0, 10.0);
    set_style_margin(sibling_style, 20.0, 0.0, 0.0, 0.0);

    init_element(&root, root_style);
    init_element(&parent, parent_style);
    init_element(&empty, empty_style);
    init_element(&child, child_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&parent));
    append_dom_child(lxb_dom_interface_node(&parent),
                     lxb_dom_interface_node(&empty));
    append_dom_child(lxb_dom_interface_node(&parent),
                     lxb_dom_interface_node(&child));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "parent-child margin collapse contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "parent-child margin collapse builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 140.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "parent-child margin collapse builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    parent_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    sibling_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 1));
    empty_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            parent_fragment, 0));
    child_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            parent_fragment, 1));
    check(root_fragment != NULL && parent_fragment != NULL
              && empty_fragment != NULL && child_fragment != NULL
              && sibling_fragment != NULL,
          "parent-child margin collapse resolves fragments");

    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "parent-child margin collapse exposes parent offset");
    check(offset.x == 0.0 && offset.y == 35.0,
          "parent top margin collapses with leading child margins");
    check(layout_result_fragment_link_offset(result, parent_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "parent-child margin collapse exposes empty child offset");
    check(offset.x == 0.0 && offset.y == 0.0,
          "leading empty child margin collapses through parent");
    check(layout_result_fragment_link_offset(result, parent_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "parent-child margin collapse exposes child offset");
    check(offset.x == 0.0 && offset.y == 0.0,
          "first non-empty child margin also collapses through parent");
    check(layout_result_fragment_size(result, parent_fragment, &size)
              == LXB_STATUS_OK,
          "parent-child margin collapse exposes parent size");
    check(size.width == 100.0 && size.height == 10.0,
          "parent auto height excludes collapsed child margins");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "parent-child margin collapse exposes sibling offset");
    check(offset.x == 0.0 && offset.y == 85.0,
          "parent bottom margin collapses with last child and sibling margins");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (parent_style != NULL) {
        lxb_style_computed_unref(parent_style);
    }
    if (empty_style != NULL) {
        lxb_style_computed_unref(empty_style);
    }
    if (child_style != NULL) {
        lxb_style_computed_unref(child_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
block_formatting_root_margin_boundary_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *parent_style = make_style();
    lxb_style_computed_t *child_style = make_style();
    lxb_style_computed_t *flow_root_style = make_style();
    lxb_style_computed_t *flow_child_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t parent;
    lxb_dom_element_t child;
    lxb_dom_element_t flow_root;
    lxb_dom_element_t flow_child;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *parent_fragment = NULL;
    layout_fragment_t *child_fragment = NULL;
    layout_fragment_t *flow_root_fragment = NULL;
    layout_fragment_t *flow_child_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && parent_style != NULL && child_style != NULL
              && flow_root_style != NULL && flow_child_style != NULL,
          "BFC margin boundary styles allocate");
    if (root_style == NULL || parent_style == NULL || child_style == NULL
        || flow_root_style == NULL || flow_child_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(parent_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(flow_root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW_ROOT);
    set_style_display(flow_child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 120.0);
    set_style_width(parent_style, 100.0);
    set_style_size(child_style, 40.0, 10.0);
    set_style_margin(child_style, 20.0, 0.0, 0.0, 0.0);
    set_style_width(flow_root_style, 100.0);
    set_style_size(flow_child_style, 40.0, 10.0);
    set_style_margin(flow_child_style, 20.0, 0.0, 0.0, 0.0);

    init_element(&root, root_style);
    init_element(&parent, parent_style);
    init_element(&child, child_style);
    init_element(&flow_root, flow_root_style);
    init_element(&flow_child, flow_child_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&parent));
    append_dom_child(lxb_dom_interface_node(&parent),
                     lxb_dom_interface_node(&child));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&flow_root));
    append_dom_child(lxb_dom_interface_node(&flow_root),
                     lxb_dom_interface_node(&flow_child));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "BFC margin boundary contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "BFC margin boundary builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 120.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "BFC margin boundary builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    parent_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    flow_root_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 1));
    child_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            parent_fragment));
    flow_child_fragment =
        layout_fragment_link_fragment(layout_fragment_first_child_link(
            flow_root_fragment));
    check(parent_fragment != NULL && child_fragment != NULL
              && flow_root_fragment != NULL && flow_child_fragment != NULL,
          "BFC margin boundary resolves fragments");

    offset = layout_fragment_link_offset(layout_fragment_first_child_link(
        parent_fragment));
    check(offset.x == 0.0 && offset.y == 0.0,
          "non-BFC child top margin collapses through parent");
    check(layout_result_fragment_size(result, parent_fragment, &size)
              == LXB_STATUS_OK,
          "non-BFC parent exposes collapsed height");
    check(size.width == 100.0 && size.height == 10.0,
          "non-BFC auto height excludes collapsed child top margin");

    offset = layout_fragment_link_offset(layout_fragment_first_child_link(
        flow_root_fragment));
    check(offset.x == 0.0 && offset.y == 20.0,
          "flow-root child top margin stays inside BFC root");
    check(layout_result_fragment_size(result, flow_root_fragment, &size)
              == LXB_STATUS_OK,
          "flow-root parent exposes contained margin height");
    check(size.width == 100.0 && size.height == 30.0,
          "BFC root auto height includes child top margin");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (parent_style != NULL) {
        lxb_style_computed_unref(parent_style);
    }
    if (child_style != NULL) {
        lxb_style_computed_unref(child_style);
    }
    if (flow_root_style != NULL) {
        lxb_style_computed_unref(flow_root_style);
    }
    if (flow_child_style != NULL) {
        lxb_style_computed_unref(flow_child_style);
    }
}

static void
auto_margin_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *child_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t child;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_point_t offset;

    check(root_style != NULL && child_style != NULL,
          "auto margin reflow styles allocate");
    if (root_style == NULL || child_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(child_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 80.0);
    set_style_size(child_style, 80.0, 10.0);
    set_style_margin_auto(child_style, LXB_STYLE_EDGE_LEFT);
    set_style_margin_auto(child_style, LXB_STYLE_EDGE_RIGHT);

    init_element(&root, root_style);
    init_element(&child, child_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&child));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "auto margin reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "auto margin reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 80.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "auto margin reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "auto margin reflow exposes child offset");
    check(offset.x == 60.0 && offset.y == 0.0,
          "horizontal auto margins split remaining containing width");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (child_style != NULL) {
        lxb_style_computed_unref(child_style);
    }
}

static void
float_clear_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *left_float_style = make_style();
    lxb_style_computed_t *right_float_style = make_style();
    lxb_style_computed_t *normal_style = make_style();
    lxb_style_computed_t *clear_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t left_float;
    lxb_dom_element_t right_float;
    lxb_dom_element_t normal;
    lxb_dom_element_t cleared;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_link_t *left_link = NULL;
    layout_fragment_link_t *right_link = NULL;
    layout_point_t offset;

    check(root_style != NULL && left_float_style != NULL
              && right_float_style != NULL && normal_style != NULL
              && clear_style != NULL,
          "float clear reflow styles allocate");
    if (root_style == NULL || left_float_style == NULL
        || right_float_style == NULL || normal_style == NULL
        || clear_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(left_float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(right_float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(normal_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(clear_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 100.0);
    set_style_size(left_float_style, 40.0, 20.0);
    set_style_float(left_float_style, LXB_CSS_FLOAT_LEFT);
    set_style_size(right_float_style, 30.0, 10.0);
    set_style_float(right_float_style, LXB_CSS_FLOAT_RIGHT);
    set_style_size(normal_style, 50.0, 10.0);
    set_style_size(clear_style, 50.0, 10.0);
    set_style_clear(clear_style, LXB_CSS_CLEAR_LEFT);

    init_element(&root, root_style);
    init_element(&left_float, left_float_style);
    init_element(&right_float, right_float_style);
    init_element(&normal, normal_style);
    init_element(&cleared, clear_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&right_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&normal));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&cleared));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "float clear reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "float clear reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 100.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "float clear reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && layout_fragment_child_count(root_fragment) == 4,
          "float clear reflow keeps floats in fragment links");

    left_link = layout_fragment_child_link_at(root_fragment, 0);
    right_link = layout_fragment_child_link_at(root_fragment, 1);
    check(layout_fragment_link_placement(left_link)
                  == LAYOUT_FRAGMENT_PLACEMENT_FLOAT
              && layout_fragment_link_placement(right_link)
                     == LAYOUT_FRAGMENT_PLACEMENT_FLOAT,
          "floating children expose float placement");
    offset = layout_fragment_link_offset(left_link);
    check(offset.x == 0.0 && offset.y == 0.0,
          "left float is placed at the current left float edge");
    offset = layout_fragment_link_offset(right_link);
    check(offset.x == 170.0 && offset.y == 0.0,
          "right float is placed at the current right float edge");

    check(layout_result_fragment_link_offset(result, root_fragment, 2,
                                             &offset)
              == LXB_STATUS_OK,
          "float clear reflow exposes normal block offset");
    check(offset.x == 40.0 && offset.y == 0.0,
          "normal block starts beside active left float");
    check(layout_result_fragment_link_offset(result, root_fragment, 3,
                                             &offset)
              == LXB_STATUS_OK,
          "float clear reflow exposes cleared block offset");
    check(offset.x == 0.0 && offset.y == 20.0,
          "clear:left moves block below active left float");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (left_float_style != NULL) {
        lxb_style_computed_unref(left_float_style);
    }
    if (right_float_style != NULL) {
        lxb_style_computed_unref(right_float_style);
    }
    if (normal_style != NULL) {
        lxb_style_computed_unref(normal_style);
    }
    if (clear_style != NULL) {
        lxb_style_computed_unref(clear_style);
    }
}

static void
clearance_breaks_margin_collapse_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *float_style = make_style();
    lxb_style_computed_t *clear_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t left_float;
    lxb_dom_element_t cleared;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_point_t offset;

    check(root_style != NULL && float_style != NULL && clear_style != NULL
              && sibling_style != NULL,
          "clearance collapse styles allocate");
    if (root_style == NULL || float_style == NULL || clear_style == NULL
        || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(clear_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 100.0);
    set_style_size(float_style, 40.0, 20.0);
    set_style_float(float_style, LXB_CSS_FLOAT_LEFT);
    set_style_width(clear_style, 50.0);
    set_style_margin(clear_style, 10.0, 0.0, 30.0, 0.0);
    set_style_clear(clear_style, LXB_CSS_CLEAR_LEFT);
    set_style_size(sibling_style, 50.0, 10.0);
    set_style_margin(sibling_style, 25.0, 0.0, 0.0, 0.0);

    init_element(&root, root_style);
    init_element(&left_float, float_style);
    init_element(&cleared, clear_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&cleared));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "clearance collapse contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "clearance collapse builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 100.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "clearance collapse builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && layout_fragment_child_count(root_fragment) == 3,
          "clearance collapse creates expected fragments");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "clearance collapse exposes cleared offset");
    check(offset.x == 0.0 && offset.y == 20.0,
          "clearance moves empty block below the active float");
    check(layout_result_fragment_link_offset(result, root_fragment, 2,
                                             &offset)
              == LXB_STATUS_OK,
          "clearance collapse exposes following sibling offset");
    check(offset.x == 0.0 && offset.y == 50.0,
          "clearance prevents empty block margins from collapsing through");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (float_style != NULL) {
        lxb_style_computed_unref(float_style);
    }
    if (clear_style != NULL) {
        lxb_style_computed_unref(clear_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
float_escape_from_non_bfc_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *parent_style = make_style();
    lxb_style_computed_t *float_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t parent;
    lxb_dom_element_t left_float;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *parent_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && parent_style != NULL && float_style != NULL
              && sibling_style != NULL,
          "float escape styles allocate");
    if (root_style == NULL || parent_style == NULL || float_style == NULL
        || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(parent_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 80.0);
    set_style_width(parent_style, 100.0);
    set_style_size(float_style, 40.0, 20.0);
    set_style_float(float_style, LXB_CSS_FLOAT_LEFT);
    set_style_size(sibling_style, 50.0, 10.0);

    init_element(&root, root_style);
    init_element(&parent, parent_style);
    init_element(&left_float, float_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&parent));
    append_dom_child(lxb_dom_interface_node(&parent),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "float escape contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "float escape builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 80.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "float escape builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    parent_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    check(root_fragment != NULL && parent_fragment != NULL
              && layout_fragment_child_count(parent_fragment) == 1,
          "float escape resolves parent and nested float fragments");
    check(layout_result_fragment_size(result, parent_fragment, &size)
              == LXB_STATUS_OK,
          "float escape exposes parent size");
    check(size.width == 100.0 && size.height == 0.0,
          "non-BFC parent does not contain its internal float height");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "float escape exposes following sibling offset");
    check(offset.x == 40.0 && offset.y == 0.0,
          "escaped float affects following sibling in the same BFC");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (parent_style != NULL) {
        lxb_style_computed_unref(parent_style);
    }
    if (float_style != NULL) {
        lxb_style_computed_unref(float_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
float_escape_uses_non_bfc_content_offset_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *parent_style = make_style();
    lxb_style_computed_t *float_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t parent;
    lxb_dom_element_t left_float;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *parent_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && parent_style != NULL && float_style != NULL
              && sibling_style != NULL,
          "float escape content offset styles allocate");
    if (root_style == NULL || parent_style == NULL || float_style == NULL
        || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(parent_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 80.0);
    set_style_width(parent_style, 100.0);
    set_style_padding(parent_style, 20.0, 0.0, 0.0, 0.0);
    set_style_size(float_style, 40.0, 20.0);
    set_style_float(float_style, LXB_CSS_FLOAT_LEFT);
    set_style_size(sibling_style, 50.0, 10.0);

    init_element(&root, root_style);
    init_element(&parent, parent_style);
    init_element(&left_float, float_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&parent));
    append_dom_child(lxb_dom_interface_node(&parent),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "float escape content offset contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "float escape content offset builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 80.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "float escape content offset builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    parent_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    check(root_fragment != NULL && parent_fragment != NULL
              && layout_fragment_child_count(parent_fragment) == 1,
          "float escape content offset resolves fragments");
    check(layout_result_fragment_size(result, parent_fragment, &size)
              == LXB_STATUS_OK,
          "float escape content offset exposes parent size");
    check(size.width == 100.0 && size.height == 20.0,
          "non-BFC parent height contains padding but not escaped float");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "float escape content offset exposes following sibling offset");
    check(offset.x == 40.0 && offset.y == 20.0,
          "escaped float starts at the non-BFC parent's content block offset");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (parent_style != NULL) {
        lxb_style_computed_unref(parent_style);
    }
    if (float_style != NULL) {
        lxb_style_computed_unref(float_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
flow_root_contains_float_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *flow_root_style = make_style();
    lxb_style_computed_t *float_style = make_style();
    lxb_style_computed_t *sibling_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t flow_root;
    lxb_dom_element_t left_float;
    lxb_dom_element_t sibling;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_fragment_t *flow_root_fragment = NULL;
    layout_size_t size;
    layout_point_t offset;

    check(root_style != NULL && flow_root_style != NULL && float_style != NULL
              && sibling_style != NULL,
          "flow-root float styles allocate");
    if (root_style == NULL || flow_root_style == NULL || float_style == NULL
        || sibling_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(flow_root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW_ROOT);
    set_style_display(float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(sibling_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 80.0);
    set_style_width(flow_root_style, 100.0);
    set_style_size(float_style, 40.0, 20.0);
    set_style_float(float_style, LXB_CSS_FLOAT_LEFT);
    set_style_size(sibling_style, 50.0, 10.0);

    init_element(&root, root_style);
    init_element(&flow_root, flow_root_style);
    init_element(&left_float, float_style);
    init_element(&sibling, sibling_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&flow_root));
    append_dom_child(lxb_dom_interface_node(&flow_root),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&sibling));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "flow-root float contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "flow-root float builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 80.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "flow-root float builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    flow_root_fragment =
        layout_fragment_link_fragment(layout_fragment_child_link_at(
            root_fragment, 0));
    check(root_fragment != NULL && flow_root_fragment != NULL
              && layout_fragment_child_count(flow_root_fragment) == 1,
          "flow-root float resolves parent and nested float fragments");
    check(layout_result_fragment_size(result, flow_root_fragment, &size)
              == LXB_STATUS_OK,
          "flow-root float exposes flow-root size");
    check(size.width == 100.0 && size.height == 20.0,
          "flow-root contains its internal float height");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &offset)
              == LXB_STATUS_OK,
          "flow-root float exposes following sibling offset");
    check(offset.x == 0.0 && offset.y == 20.0,
          "flow-root isolates internal float from following sibling");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (flow_root_style != NULL) {
        lxb_style_computed_unref(flow_root_style);
    }
    if (float_style != NULL) {
        lxb_style_computed_unref(float_style);
    }
    if (sibling_style != NULL) {
        lxb_style_computed_unref(sibling_style);
    }
}

static void
float_available_space_reflow_smoke(layout_t *layout)
{
    lxb_style_computed_t *root_style = make_style();
    lxb_style_computed_t *left_float_style = make_style();
    lxb_style_computed_t *right_float_style = make_style();
    lxb_style_computed_t *normal_style = make_style();
    lxb_dom_element_t root;
    lxb_dom_element_t left_float;
    lxb_dom_element_t right_float;
    lxb_dom_element_t normal;
    layout_tree_t *tree = NULL;
    layout_result_t *result = NULL;
    layout_fragment_builder_t builder;
    layout_fragment_t *root_fragment = NULL;
    layout_point_t offset;

    check(root_style != NULL && left_float_style != NULL
              && right_float_style != NULL && normal_style != NULL,
          "float available space reflow styles allocate");
    if (root_style == NULL || left_float_style == NULL
        || right_float_style == NULL || normal_style == NULL) {
        goto done;
    }

    set_style_display(root_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(left_float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(right_float_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_display(normal_style, LXB_CSS_PROPERTY__UNDEF,
                      LXB_CSS_DISPLAY_BLOCK, LXB_CSS_DISPLAY_FLOW);
    set_style_size(root_style, 200.0, 80.0);
    set_style_size(left_float_style, 90.0, 20.0);
    set_style_float(left_float_style, LXB_CSS_FLOAT_LEFT);
    set_style_size(right_float_style, 90.0, 10.0);
    set_style_float(right_float_style, LXB_CSS_FLOAT_RIGHT);
    set_style_size(normal_style, 30.0, 10.0);

    init_element(&root, root_style);
    init_element(&left_float, left_float_style);
    init_element(&right_float, right_float_style);
    init_element(&normal, normal_style);
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&left_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&right_float));
    append_dom_child(lxb_dom_interface_node(&root),
                     lxb_dom_interface_node(&normal));

    tree = layout_tree_create(layout);
    result = layout_result_create(layout);
    check(tree != NULL && result != NULL,
          "float available space reflow contexts allocate");
    if (tree == NULL || result == NULL) {
        goto done;
    }

    check(layout_tree_build(tree, lxb_dom_interface_node(&root))
              == LXB_STATUS_OK,
          "float available space reflow builds layout tree");

    memset(&builder, 0, sizeof(builder));
    builder.viewport_size.width = 200.0;
    builder.viewport_size.height = 80.0;

    check(layout_tree_build_fragments(tree, result, &builder)
              == LXB_STATUS_OK,
          "float available space reflow builds physical fragments");
    freeze_result(result);

    root_fragment = layout_result_root_fragment(result);
    check(root_fragment != NULL && layout_fragment_child_count(root_fragment) == 3,
          "float available space reflow creates expected fragments");
    check(layout_result_fragment_link_offset(result, root_fragment, 2,
                                             &offset)
              == LXB_STATUS_OK,
          "float available space reflow exposes normal block offset");
    check(offset.x == 90.0 && offset.y == 10.0,
          "normal block moves to first y with enough float-free width");

done:
    layout_result_destroy(result, true);
    layout_tree_destroy(tree, true);

    if (root_style != NULL) {
        lxb_style_computed_unref(root_style);
    }
    if (left_float_style != NULL) {
        lxb_style_computed_unref(left_float_style);
    }
    if (right_float_style != NULL) {
        lxb_style_computed_unref(right_float_style);
    }
    if (normal_style != NULL) {
        lxb_style_computed_unref(normal_style);
    }
}

static void
layout_object_tree_smoke(layout_t *layout)
{
    layout_object_child_list_t children;
    layout_object_child_list_t other_children;
    layout_object_child_list_t nested_children;
    layout_object_t *parent = make_object(layout);
    layout_object_t *first = make_object(layout);
    layout_object_t *second = make_object(layout);
    layout_object_t *inserted = make_object(layout);
    layout_object_t *new_first = make_object(layout);
    layout_object_t *nested = make_object(layout);
    layout_object_t *nested_inserted = make_object(layout);
    layout_object_t *stray = make_object(layout);
    layout_object_t *rejected = make_object(layout);
    layout_object_t *detached = make_object(layout);
    layout_object_common_ancestor_data_t ancestor_data;

    layout_object_child_list_init(&children);
    layout_object_child_list_init(&other_children);
    layout_object_child_list_init(&nested_children);

    check(parent != NULL && first != NULL && second != NULL
              && inserted != NULL && new_first != NULL && nested != NULL
              && nested_inserted != NULL && stray != NULL && rejected != NULL
              && detached != NULL,
          "layout object tree objects allocate");
    check(layout_object_child_list_append(&children, parent, first)
              == LXB_STATUS_OK,
          "append first child to external child list");
    check(layout_object_child_list_append(&children, parent, second)
              == LXB_STATUS_OK,
          "append second child to external child list");
    check(layout_object_slow_first_child(&children) == first,
          "child list exposes first child");
    check(layout_object_slow_last_child(&children) == second,
          "child list exposes last child");
    check(layout_object_parent(first) == parent,
          "first child parent is set");
    check(layout_object_parent(second) == parent,
          "second child parent is set");
    check(layout_object_next_sibling(first) == second,
          "first child next sibling is second child");
    check(layout_object_previous_sibling(second) == first,
          "second child previous sibling is first child");
    check(layout_object_child_needs_full_layout(parent),
          "child insertion marks owner child layout dirty");
    layout_object_clear_needs_layout(parent);
    check(layout_object_child_list_insert_before(&children, parent, inserted,
                                                 second)
              == LXB_STATUS_OK,
          "insert child before direct child");
    check(layout_object_next_sibling(first) == inserted,
          "insert before updates previous sibling next link");
    check(layout_object_previous_sibling(inserted) == first,
          "inserted child previous sibling is linked");
    check(layout_object_next_sibling(inserted) == second,
          "inserted child next sibling is linked");
    check(layout_object_previous_sibling(second) == inserted,
          "insert before updates before-child previous link");
    check(layout_object_slow_last_child(&children) == second,
          "middle insert preserves last child");
    check(layout_object_child_list_insert_before(&children, parent,
                                                 new_first, first)
              == LXB_STATUS_OK,
          "insert child before first child");
    check(layout_object_slow_first_child(&children) == new_first,
          "insert before first updates first child");
    check(layout_object_next_sibling(new_first) == first,
          "new first links to old first");
    check(layout_object_previous_sibling(first) == new_first,
          "old first previous sibling is new first");
    check(layout_object_child_list_append(&nested_children, inserted, nested)
              == LXB_STATUS_OK,
          "append nested child under inserted child");
    check(layout_object_child_list_insert_before(&children, parent,
                                                 nested_inserted, nested)
              == LXB_STATUS_OK,
          "insert before descendant normalizes to direct child");
    check(layout_object_next_sibling(first) == nested_inserted,
          "descendant before insert lands before ancestor child");
    check(layout_object_next_sibling(nested_inserted) == inserted,
          "descendant before insert links to normalized direct child");
    check(layout_object_depth(parent) == 1
              && layout_object_depth(nested) == 3,
          "layout object depth counts parent chain");
    memset(&ancestor_data, 0xff, sizeof(ancestor_data));
    check(layout_object_common_ancestor(first, nested, &ancestor_data)
              == parent,
          "common ancestor finds nearest layout parent");
    check(ancestor_data.last == first
              && ancestor_data.other_last == inserted,
          "common ancestor reports branch children below ancestor");
    check(layout_object_common_ancestor(nested, nested, &ancestor_data)
              == nested,
          "common ancestor of same object is itself");
    check(ancestor_data.last == NULL && ancestor_data.other_last == NULL,
          "same-object common ancestor has no branch children");
    check(layout_object_common_ancestor(first, detached, NULL) == NULL,
          "detached tree objects have no common ancestor");
    check(layout_object_is_before_in_preorder(parent, nested),
          "preorder treats ancestor before descendant");
    check(!layout_object_is_before_in_preorder(nested, parent),
          "preorder treats descendant after ancestor");
    check(layout_object_is_before_in_preorder(first, nested),
          "preorder follows sibling branch order");
    check(layout_object_is_before_in_preorder(nested_inserted, nested),
          "preorder compares descendant against later sibling branch");
    check(!layout_object_is_before_in_preorder(nested, nested_inserted),
          "preorder rejects later branch before earlier branch");
    check(!layout_object_is_before_in_preorder(first, first),
          "preorder does not order an object before itself");
    check(layout_object_child_list_append(&other_children, parent, stray)
              == LXB_STATUS_OK,
          "append child to another list with same parent");
    check(layout_object_child_list_insert_before(&children, parent, rejected,
                                                 stray)
              == LXB_STATUS_ERROR_NOT_EXISTS,
          "insert before rejects child outside this list");
    check(layout_object_parent(rejected) == NULL,
          "failed insert leaves rejected child detached");
    check(layout_object_child_list_append(&children, parent, second)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "child list rejects already-parented child");
    check(layout_object_child_list_remove(&other_children, first)
              == LXB_STATUS_ERROR_NOT_EXISTS,
          "child list rejects removing child from another list");
    check(layout_object_parent(first) == parent,
          "failed remove leaves child parent intact");
    check(layout_object_child_list_remove(&children, first) == LXB_STATUS_OK,
          "child list removes middle child");
    check(layout_object_slow_first_child(&children) == new_first,
          "middle remove preserves first child");
    check(layout_object_next_sibling(new_first) == nested_inserted,
          "middle remove reconnects previous sibling next link");
    check(layout_object_previous_sibling(nested_inserted) == new_first,
          "middle remove reconnects next sibling previous link");
    check(layout_object_child_needs_full_layout(parent),
          "child removal marks owner child layout dirty");
    check(layout_object_previous_sibling(first) == NULL
              && layout_object_next_sibling(first) == NULL,
          "remove clears removed child sibling links");
    check(layout_object_parent(first) == NULL,
          "remove clears removed child parent");
}

static void
layout_object_child_list_oof_dirty_smoke(layout_t *layout)
{
    layout_object_child_list_t flow_children;
    layout_object_child_list_t column_children;
    layout_object_t *root = make_object(layout);
    layout_object_t *flow_parent = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_object_t *column_container = make_object(layout);
    layout_object_t *column_spanner = make_object(layout);

    layout_object_child_list_init(&flow_children);
    layout_object_child_list_init(&column_children);

    check(root != NULL && flow_parent != NULL && absolute_child != NULL
              && column_container != NULL && column_spanner != NULL,
          "oof child-list dirty objects allocate");
    check(layout_object_set_parent(flow_parent, root) == LXB_STATUS_OK,
          "oof dirty set flow parent ancestor");
    check(layout_object_set_parent(column_container, root) == LXB_STATUS_OK,
          "spanner dirty set column container ancestor");
    layout_object_set_can_contain_absolute_position(root, true);
    layout_object_set_column_spanner_container(column_container, true);
    set_position(absolute_child, LXB_CSS_POSITION_ABSOLUTE);
    layout_object_set_column_spanner(column_spanner, true);

    check(layout_object_child_list_append(&flow_children, flow_parent,
                                          absolute_child)
              == LXB_STATUS_OK,
          "append oof child through child list");
    check(layout_object_child_needs_full_layout(flow_parent),
          "oof insertion marks layout parent dirty");
    check(layout_object_child_needs_full_layout(root),
          "oof insertion marks containing block dirty");

    layout_object_clear_needs_layout(flow_parent);
    layout_object_clear_needs_layout(root);
    check(layout_object_child_list_remove(&flow_children, absolute_child)
              == LXB_STATUS_OK,
          "remove oof child through child list");
    check(layout_object_child_needs_full_layout(flow_parent),
          "oof removal marks layout parent dirty");
    check(layout_object_child_needs_full_layout(root),
          "oof removal marks containing block dirty");

    layout_object_clear_needs_layout(root);
    check(layout_object_child_list_append(&column_children,
                                          column_container,
                                          column_spanner)
              == LXB_STATUS_OK,
          "append column spanner through child list");
    check(layout_object_child_needs_full_layout(column_container),
          "column spanner insertion marks multicol container dirty");
    check((layout_object_dirty_bits(root) & LAYOUT_DIRTY_CHILD_FULL) != 0,
          "column spanner insertion propagates dirty chain");
}

static void
layout_object_child_list_clear_smoke(layout_t *layout)
{
    layout_object_child_list_t children;
    layout_object_t *parent = make_object(layout);
    layout_object_t *first = make_object(layout);
    layout_object_t *second = make_object(layout);

    layout_object_child_list_init(&children);

    check(parent != NULL && first != NULL && second != NULL,
          "child-list clear objects allocate");
    check(layout_object_child_list_append(&children, parent, first)
              == LXB_STATUS_OK,
          "clear smoke appends first child");
    check(layout_object_child_list_append(&children, parent, second)
              == LXB_STATUS_OK,
          "clear smoke appends second child");
    layout_object_clear_needs_layout(parent);

    check(layout_object_child_list_clear(&children, parent) == LXB_STATUS_OK,
          "child-list clear removes all children");
    check(layout_object_slow_first_child(&children) == NULL,
          "child-list clear resets first child");
    check(layout_object_slow_last_child(&children) == NULL,
          "child-list clear resets last child");
    check(layout_object_parent(first) == NULL
              && layout_object_parent(second) == NULL,
          "child-list clear detaches children");
    check(layout_object_previous_sibling(first) == NULL
              && layout_object_next_sibling(first) == NULL
              && layout_object_previous_sibling(second) == NULL
              && layout_object_next_sibling(second) == NULL,
          "child-list clear detaches sibling links");
    check(layout_object_child_needs_full_layout(parent),
          "child-list clear marks owner dirty");
}

static void
layout_object_state_smoke(layout_t *layout)
{
    layout_object_t *object = make_object(layout);
    layout_object_state_t state;

    check(object != NULL, "layout object state object allocates");
    set_position(object, LXB_CSS_POSITION_ABSOLUTE);
    set_overflow(object, LXB_CSS_OVERFLOW_X_HIDDEN,
                 LXB_CSS_OVERFLOW_Y_SCROLL);
    set_visibility(object, LXB_CSS_VISIBILITY_HIDDEN);
    set_z_index(object, 12);

    check(layout_object_state(object, &state) == LXB_STATUS_OK,
          "layout object state reads computed style");
    check(state.position == LXB_CSS_POSITION_ABSOLUTE,
          "layout object state exposes position");
    check(state.overflow_x == LXB_CSS_OVERFLOW_X_HIDDEN
              && state.overflow_y == LXB_CSS_OVERFLOW_Y_SCROLL,
          "layout object state exposes overflow axes");
    check(state.visibility == LXB_CSS_VISIBILITY_HIDDEN,
          "layout object state exposes visibility");
    check(state.has_z_index && state.z_index == 12,
          "layout object state exposes z-index");
    check(state.transform.a == 1.0 && state.transform.d == 1.0
              && state.transform.e == 0.0 && state.transform.f == 0.0,
          "layout object state reserves identity transform");
}

static void
containing_block_smoke(layout_t *layout)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *static_parent = make_object(layout);
    layout_object_t *relative_parent = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_object_t *fixed_child = make_object(layout);
    layout_object_t *fixed_container = make_object(layout);
    layout_object_t *transformed_fixed = make_object(layout);
    layout_object_t *column_container = make_object(layout);
    layout_object_t *column_spanner = make_object(layout);
    layout_object_t *inside_spanner = make_object(layout);
    layout_object_t *column_child = make_object(layout);

    check(root != NULL && static_parent != NULL && relative_parent != NULL
              && absolute_child != NULL && fixed_child != NULL
              && fixed_container != NULL && transformed_fixed != NULL
              && column_container != NULL && column_spanner != NULL
              && inside_spanner != NULL && column_child != NULL,
          "layout objects allocate");

    check(layout_object_set_parent(static_parent, root) == LXB_STATUS_OK,
          "set static parent ancestor");
    check(layout_object_set_parent(relative_parent, static_parent)
              == LXB_STATUS_OK,
          "set relative parent ancestor");
    check(layout_object_set_parent(absolute_child, relative_parent)
              == LXB_STATUS_OK,
          "set absolute child ancestor");
    check(layout_object_set_parent(fixed_child, static_parent)
              == LXB_STATUS_OK,
          "set fixed child ancestor");
    check(layout_object_set_parent(fixed_container, static_parent)
              == LXB_STATUS_OK,
          "set fixed container ancestor");
    check(layout_object_set_parent(transformed_fixed, fixed_container)
              == LXB_STATUS_OK,
          "set fixed under fixed container ancestor");
    check(layout_object_set_parent(column_container, static_parent)
              == LXB_STATUS_OK,
          "set column container ancestor");
    check(layout_object_set_parent(column_spanner, column_container)
              == LXB_STATUS_OK,
          "set column spanner ancestor");
    check(layout_object_set_parent(inside_spanner, column_spanner)
              == LXB_STATUS_OK,
          "set child inside column spanner ancestor");
    check(layout_object_set_parent(column_child, column_container)
              == LXB_STATUS_OK,
          "set column child ancestor");

    set_position(relative_parent, LXB_CSS_POSITION_RELATIVE);
    set_position(absolute_child, LXB_CSS_POSITION_ABSOLUTE);
    set_position(fixed_child, LXB_CSS_POSITION_FIXED);
    set_position(transformed_fixed, LXB_CSS_POSITION_FIXED);
    layout_object_set_can_contain_absolute_position(root, true);
    layout_object_set_can_contain_fixed_position(root, true);
    layout_object_set_can_contain_absolute_position(relative_parent, true);
    layout_object_set_can_contain_fixed_position(fixed_container, true);
    layout_object_set_column_spanner_container(column_container, true);
    layout_object_set_column_spanner(column_spanner, true);

    check(!layout_object_can_contain_out_of_flow_positioned(static_parent,
                                                            false),
          "static parent does not contain absolute positioned descendants");
    check(layout_object_can_contain_out_of_flow_positioned(relative_parent,
                                                           false),
          "relative parent can contain absolute positioned descendants");
    check(layout_object_can_contain_out_of_flow_positioned(fixed_container,
                                                           true),
          "fixed container can contain fixed positioned descendants");
    check(layout_object_containing_block_for_absolute(absolute_child)
              == relative_parent,
          "absolute containing block uses cached absolute-container bit");
    check(layout_object_containing_block_for_fixed(fixed_child) == root,
          "fixed containing block falls back to root");
    check(layout_object_containing_block_for_fixed(transformed_fixed)
              == fixed_container,
          "fixed containing block uses cached fixed-container bit");
    check(layout_object_containing_block_for_column_spanner(column_child)
              == column_container,
          "column spanner containing block reserved hook works");
    check(layout_object_containing_block_for_column_spanner(column_spanner)
              == column_container,
          "column spanner finds its multicol container");
    check(layout_object_containing_block_for_absolute(inside_spanner)
              == root,
          "absolute containing block walker skips column spanner to container");
}

static void
containing_block_cached_bit_smoke(layout_t *layout)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);

    check(root != NULL && child != NULL,
          "containing block cached bit objects allocate");
    check(layout_object_set_parent(child, root) == LXB_STATUS_OK,
          "containing block cached bit set parent");

    check(layout_object_containing_block_for_absolute(child) == NULL,
          "absolute containing block does not use root without cached bit");
    check(layout_object_containing_block_for_fixed(child) == NULL,
          "fixed containing block does not use root without cached bit");

    layout_object_set_can_contain_absolute_position(root, true);
    check(layout_object_containing_block_for_absolute(child) == root,
          "absolute containing block uses root cached bit");

    layout_object_set_can_contain_fixed_position(root, true);
    check(layout_object_containing_block_for_fixed(child) == root,
          "fixed containing block uses root cached bit");
}

static void
dirty_guard_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_fragment_t *root_fragment;
    layout_fragment_t *child_fragment;
    layout_fragment_t *hit = NULL;
    layout_point_t local = make_point(0.0, 0.0);
    layout_size_t size;
    layout_rect_t rect;
    layout_point_t offset;
    layout_fragment_t *mapped_fragment = NULL;

    check(layout_object_set_parent(child, root) == LXB_STATUS_OK,
          "dirty smoke set parent");

    root_fragment = make_fragment(result, root, 200.0, 200.0);
    child_fragment = make_fragment(result, child, 50.0, 50.0);
    check(root_fragment != NULL && child_fragment != NULL,
          "dirty smoke fragments allocate");
    check(layout_fragment_append_child(root_fragment, child_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "dirty smoke append fragment");

    layout_object_set_self_needs_full_layout(child, true);
    check((layout_object_dirty_bits(root) & LAYOUT_DIRTY_CHILD_FULL) != 0,
          "child full layout dirties ancestor");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "layout result accepts root physical fragment");
    check(layout_result_root_fragment(result) == root_fragment,
          "layout result stores root physical fragment");
    freeze_result(result);

    check(layout_hit_test(result, root_fragment, make_point(20.0, 20.0),
                          &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty layout blocks hit test");
    check(hit == NULL, "dirty hit test returns no fragment");
    check(layout_result_needs_layout(result),
          "layout result reports dirty root object");
    check(layout_result_point_in_fragment(result, child_fragment,
                                          make_point(20.0, 20.0),
                                          &local)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty layout blocks fragment coordinate query");
    check(layout_result_fragment_size(result, child_fragment, &size)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty layout blocks fragment size query");
    check(layout_result_fragment_local_rect(result, child_fragment, &rect)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty layout blocks fragment local rect query");
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty layout blocks fragment link offset query");

    layout_object_clear_needs_layout(child);
    layout_object_clear_needs_layout(root);
    check(!layout_result_needs_layout(result),
          "layout result is clean after clearing dirty bits");
    check(layout_fragment_append_child(root_fragment, child_fragment,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "frozen result rejects fragment tree mutation");
    check(layout_hit_test(result, NULL, make_point(20.0, 20.0),
                          &hit)
              == LXB_STATUS_OK,
          "clean layout allows hit test through result root");
    check(hit == child_fragment, "clean hit test reaches child fragment");
    check(layout_result_point_in_fragment(result, child_fragment,
                                          make_point(20.0, 20.0),
                                          &local)
              == LXB_STATUS_OK,
          "clean layout allows fragment coordinate query");
    check(local.x == 10.0 && local.y == 10.0,
          "result coordinate query maps root point to child local point");
    check(layout_result_root_point_to_object(result, child, 0,
                                             make_point(20.0, 20.0),
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &local, &mapped_fragment)
              == LXB_STATUS_OK,
          "root point maps to child layout object local coordinates");
    check(mapped_fragment == child_fragment,
          "object coordinate query returns matching physical fragment");
    check(local.x == 10.0 && local.y == 10.0,
          "object coordinate query uses fragment link offset");
    check(layout_result_fragment_size(result, child_fragment, &size)
              == LXB_STATUS_OK,
          "clean layout allows fragment size query");
    check(size.width == 50.0 && size.height == 50.0,
          "fragment size query returns physical size");
    check(layout_result_fragment_local_rect(result, child_fragment, &rect)
              == LXB_STATUS_OK,
          "clean layout allows fragment local rect query");
    check(rect.x == 0.0 && rect.y == 0.0 && rect.width == 50.0
              && rect.height == 50.0,
          "fragment local rect query returns local physical rect");
    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &offset)
              == LXB_STATUS_OK,
          "clean layout allows fragment link offset query");
    check(offset.x == 10.0 && offset.y == 10.0,
          "fragment link offset query returns child placement");
}

static void
dirty_bits_guard_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 100.0,
                                                     100.0);
    layout_fragment_t *hit = NULL;
    layout_size_t size;

    check(root_fragment != NULL, "dirty bits guard fragment allocates");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "dirty bits guard stores result root");
    freeze_result(result);

    layout_object_set_self_needs_full_layout(root, true);
    check(layout_object_self_needs_full_layout(root),
          "self full dirty bit is observable");
    check(layout_hit_test(result, NULL, make_point(1.0, 1.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "self full dirty bit blocks hit test");
    check(layout_result_fragment_size(result, root_fragment, &size)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "self full dirty bit blocks fragment query");
    layout_object_clear_needs_layout(root);

    layout_object_set_child_needs_full_layout(root, true);
    check(layout_object_child_needs_full_layout(root),
          "child full dirty bit is observable");
    check(layout_hit_test(result, NULL, make_point(1.0, 1.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "child full dirty bit blocks hit test");
    check(layout_result_fragment_size(result, root_fragment, &size)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "child full dirty bit blocks fragment query");
    layout_object_clear_needs_layout(root);

    layout_object_set_needs_simplified_layout(root, true);
    check(layout_object_needs_simplified_layout(root),
          "simplified dirty bit is observable");
    check(layout_hit_test(result, NULL, make_point(1.0, 1.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "simplified dirty bit blocks hit test");
    check(layout_result_fragment_size(result, root_fragment, &size)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "simplified dirty bit blocks fragment query");
    layout_object_clear_needs_layout(root);

    check(layout_hit_test(result, NULL, make_point(1.0, 1.0), &hit)
              == LXB_STATUS_OK,
          "cleared dirty bits allow hit test");
    check(hit == root_fragment, "clean dirty bits guard hits root");
}

static void
fragment_subtree_dirty_guard_smoke(layout_t *layout,
                                   layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 100.0,
                                                     100.0);
    layout_fragment_t *child_fragment = make_fragment(result, child, 20.0,
                                                      20.0);
    layout_fragment_t *hit = NULL;
    layout_size_t size;

    check(layout_object_set_parent(child, root) == LXB_STATUS_OK,
          "fragment subtree dirty guard set parent");
    check(root_fragment != NULL && child_fragment != NULL,
          "fragment subtree dirty guard fragments allocate");
    check(layout_fragment_append_child(root_fragment, child_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "fragment subtree dirty guard appends child fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "fragment subtree dirty guard stores result root");
    freeze_result(result);

    layout_object_set_child_needs_full_layout(child, true);
    check(layout_result_needs_layout(result),
          "dirty fragment subtree object dirties result");
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty fragment subtree blocks hit test");
    check(hit == NULL, "dirty fragment subtree returns no hit");
    check(layout_result_fragment_size(result, child_fragment, &size)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty fragment subtree blocks fragment query");

    layout_object_clear_needs_layout(child);
    check(!layout_result_needs_layout(result),
          "cleared fragment subtree dirty bit cleans result");
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_OK,
          "clean fragment subtree allows hit test");
    check(hit == child_fragment,
          "clean fragment subtree reaches child fragment");
}

static void
out_of_flow_geometry_owner_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *flow_parent = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_object_t *fixed_child = make_object(layout);
    layout_object_t *low_z_absolute = make_object(layout);
    layout_object_t *high_z_absolute = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 300.0,
                                                     300.0);
    layout_fragment_t *flow_fragment = make_fragment(result, flow_parent,
                                                     50.0, 50.0);
    layout_fragment_t *absolute_fragment = make_fragment(result,
                                                        absolute_child,
                                                        30.0, 30.0);
    layout_fragment_t *fixed_fragment = make_fragment(result, fixed_child,
                                                      40.0, 40.0);
    layout_fragment_t *low_z_absolute_fragment =
        make_fragment_with_stacking_order(result, low_z_absolute, 30.0,
                                          30.0, 1);
    layout_fragment_t *high_z_absolute_fragment =
        make_fragment_with_stacking_order(result, high_z_absolute, 30.0,
                                          30.0, 10);
    layout_oof_positioned_t *oof;
    layout_point_t offset;
    layout_point_t local;
    layout_rect_t local_rect;
    layout_rect_t root_rect;
    layout_fragment_t *hit = NULL;
    layout_fragment_t *mapped_fragment = NULL;

    check(layout_object_set_parent(flow_parent, root) == LXB_STATUS_OK,
          "set flow parent under root");
    check(layout_object_set_parent(absolute_child, flow_parent)
              == LXB_STATUS_OK,
          "absolute child keeps DOM/layout parent");
    check(layout_object_set_parent(fixed_child, flow_parent)
              == LXB_STATUS_OK,
          "fixed child keeps DOM/layout parent");
    check(layout_object_set_parent(low_z_absolute, flow_parent)
              == LXB_STATUS_OK,
          "low z absolute child keeps DOM/layout parent");
    check(layout_object_set_parent(high_z_absolute, flow_parent)
              == LXB_STATUS_OK,
          "high z absolute child keeps DOM/layout parent");
    layout_object_set_can_contain_absolute_position(root, true);
    layout_object_set_can_contain_fixed_position(root, true);

    check(layout_object_containing_block_for_absolute(absolute_child)
              == root,
          "absolute child escapes DOM parent to root containing block");
    check(layout_object_containing_block_for_fixed(fixed_child) == root,
          "fixed child escapes DOM parent to root containing block");

    check(layout_fragment_append_child(root_fragment, flow_fragment,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_OK,
          "normal-flow parent fragment is under root");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "oof smoke stores result root");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   root, absolute_fragment,
                                                   make_point(120.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "absolute child is registered under containing block geometry owner");
    check(layout_result_add_out_of_flow_positioned(result, fixed_child, root,
                                                   fixed_fragment,
                                                   make_point(10.0, 150.0),
                                                   true)
              == LXB_STATUS_OK,
          "fixed child is registered under fixed containing block owner");
    check(layout_result_add_out_of_flow_positioned(result, high_z_absolute,
                                                   root,
                                                   high_z_absolute_fragment,
                                                   make_point(190.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "high z absolute oof registers before low z oof");
    check(layout_result_add_out_of_flow_positioned(result, low_z_absolute,
                                                   root,
                                                   low_z_absolute_fragment,
                                                   make_point(190.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "low z absolute oof registers after high z oof");
    check(layout_result_out_of_flow_positioned_count(result) == 4,
          "oof registry stores positioned descendants");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   flow_parent,
                                                   absolute_fragment,
                                                   make_point(0.0, 0.0),
                                                   false)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "oof registry rejects DOM parent as wrong geometry owner");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   root, fixed_fragment,
                                                   make_point(0.0, 0.0),
                                                   false)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "oof registry rejects fragment owned by another object");
    freeze_result(result);

    oof = layout_result_out_of_flow_positioned_at(result, 0);
    check(layout_oof_positioned_object(oof) == absolute_child,
          "oof entry stores positioned object");
    check(layout_oof_positioned_containing_block(oof) == root,
          "oof entry stores geometry containing block");
    check(layout_oof_positioned_fragment(oof) == absolute_fragment,
          "oof entry stores physical fragment");
    check(!layout_oof_positioned_is_fixed(oof),
          "absolute oof entry records positioning type");
    offset = layout_oof_positioned_offset(oof);
    check(offset.x == 120.0 && offset.y == 20.0,
          "oof entry stores containing-block-relative offset");
    check(layout_oof_positioned_stacking_order(
              layout_result_out_of_flow_positioned_at(result, 2))
              == 10,
          "oof entry stores visual stacking order");

    check(layout_hit_test(result, NULL, make_point(125.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "hit test reaches absolute oof fragment via geometry owner");
    check(hit == absolute_fragment,
          "absolute oof hit does not require DOM parent fragment link");
    check(layout_result_root_point_to_object(result, absolute_child, 0,
                                             make_point(125.0, 25.0),
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &local, &mapped_fragment)
              == LXB_STATUS_OK,
          "root point maps to absolute oof object local coordinates");
    check(mapped_fragment == absolute_fragment,
          "absolute oof coordinate mapping reports physical fragment");
    check(local.x == 5.0 && local.y == 5.0,
          "absolute oof local coordinates use containing block offset");
    check(layout_hit_test(result, NULL, make_point(15.0, 155.0), &hit)
              == LXB_STATUS_OK,
          "hit test reaches fixed oof fragment via geometry owner");
    check(hit == fixed_fragment,
          "fixed oof hit does not require DOM parent fragment link");
    check(layout_result_root_point_to_object(result, fixed_child, 0,
                                             make_point(15.0, 155.0),
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &local, NULL)
              == LXB_STATUS_OK,
          "root point maps to fixed oof object local coordinates");
    check(local.x == 5.0 && local.y == 5.0,
          "fixed oof local coordinates use fixed containing block offset");
    check(layout_hit_test(result, NULL, make_point(195.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "overlapping oof fragments hit");
    check(hit == high_z_absolute_fragment,
          "oof hit test prefers stacking order over registration order");

    local_rect.x = 0.0;
    local_rect.y = 0.0;
    local_rect.width = 30.0;
    local_rect.height = 30.0;
    check(layout_result_fragment_rect_to_root(result, absolute_fragment,
                                             local_rect,
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &root_rect)
              == LXB_STATUS_OK,
          "absolute oof local rect maps to root coordinates");
    check(root_rect.x == 120.0 && root_rect.y == 20.0
              && root_rect.width == 30.0 && root_rect.height == 30.0,
          "absolute oof rect mapping uses geometry owner offset");

    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   root,
                                                   absolute_fragment,
                                                   make_point(0.0, 0.0),
                                                   false)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "oof registry rejects mutation after result freeze");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   root, fixed_fragment,
                                                   make_point(0.0, 0.0),
                                                   false)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "oof registry rejects fragment mutation after result freeze");
}

static void
out_of_flow_dirty_guard_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 200.0,
                                                     200.0);
    layout_fragment_t *absolute_fragment = make_fragment(result,
                                                        absolute_child,
                                                        30.0, 30.0);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(absolute_child, root) == LXB_STATUS_OK,
          "oof dirty guard keeps layout parent");
    layout_object_set_can_contain_absolute_position(root, true);
    check(root_fragment != NULL && absolute_fragment != NULL,
          "oof dirty guard fragments allocate");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "oof dirty guard stores result root");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   root, absolute_fragment,
                                                   make_point(20.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "oof dirty guard registers absolute fragment");
    freeze_result(result);

    layout_object_set_needs_simplified_layout(absolute_child, true);
    check(layout_result_needs_layout(result),
          "dirty oof object dirties result");
    check(layout_hit_test(result, NULL, make_point(25.0, 25.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "dirty oof object blocks hit test");
    check(hit == NULL, "dirty oof object returns no hit");

    layout_object_clear_needs_layout(absolute_child);
    check(!layout_result_needs_layout(result),
          "cleared oof dirty bit cleans result");
    check(layout_hit_test(result, NULL, make_point(25.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "clean oof object allows hit test");
    check(hit == absolute_fragment,
          "clean oof object reaches positioned fragment");
}

static void
out_of_flow_containing_fragment_smoke(layout_t *layout,
                                      layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *container = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 180.0,
                                                     80.0);
    layout_fragment_t *first_container_fragment = make_fragment(result,
                                                               container,
                                                               40.0, 40.0);
    layout_fragment_t *second_container_fragment = make_fragment(result,
                                                                container,
                                                                40.0, 40.0);
    layout_fragment_t *absolute_fragment = make_fragment(result,
                                                        absolute_child,
                                                        20.0, 20.0);
    layout_oof_positioned_t *oof;
    layout_fragment_t *hit = NULL;
    layout_point_t local;
    layout_rect_t local_rect;
    layout_rect_t root_rect;

    check(layout_object_set_parent(container, root) == LXB_STATUS_OK,
          "fragment-bound oof container parent set");
    check(layout_object_set_parent(absolute_child, container)
              == LXB_STATUS_OK,
          "fragment-bound oof child parent set");
    layout_object_set_can_contain_absolute_position(container, true);
    set_position(absolute_child, LXB_CSS_POSITION_ABSOLUTE);

    check(root_fragment != NULL && first_container_fragment != NULL
              && second_container_fragment != NULL
              && absolute_fragment != NULL,
          "fragment-bound oof fragments allocate");
    check(layout_fragment_append_child(root_fragment, first_container_fragment,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_OK,
          "fragment-bound oof appends first containing fragment");
    check(layout_fragment_append_child(root_fragment, second_container_fragment,
                                       make_point(100.0, 0.0))
              == LXB_STATUS_OK,
          "fragment-bound oof appends second containing fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "fragment-bound oof stores result root");
    check(layout_result_add_out_of_flow_positioned_in_fragment(
              result, absolute_child, container, root_fragment,
              absolute_fragment, make_point(10.0, 10.0), false)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "fragment-bound oof rejects mismatched containing fragment");
    check(layout_result_add_out_of_flow_positioned_in_fragment(
              result, absolute_child, container, second_container_fragment,
              absolute_fragment, make_point(10.0, 10.0), false)
              == LXB_STATUS_OK,
          "fragment-bound oof stores exact containing fragment");
    freeze_result(result);

    oof = layout_result_out_of_flow_positioned_at(result, 0);
    check(layout_oof_positioned_containing_block(oof) == container,
          "fragment-bound oof keeps containing block object");
    check(layout_oof_positioned_containing_fragment(oof)
              == second_container_fragment,
          "fragment-bound oof exposes containing physical fragment");
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_OK,
          "first same-object containing fragment can still be hit");
    check(hit == first_container_fragment,
          "fragment-bound oof does not attach to first same-object fragment");
    check(layout_hit_test(result, NULL, make_point(115.0, 15.0), &hit)
              == LXB_STATUS_OK,
          "second same-object containing fragment dispatches oof hit");
    check(hit == absolute_fragment,
          "fragment-bound oof uses exact containing fragment placement");

    check(layout_result_root_point_to_object(
              result, absolute_child, 0, make_point(115.0, 15.0),
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &local, NULL)
              == LXB_STATUS_OK,
          "fragment-bound oof maps root point through exact fragment");
    check(local.x == 5.0 && local.y == 5.0,
          "fragment-bound oof root point uses second fragment offset");

    local_rect.x = 0.0;
    local_rect.y = 0.0;
    local_rect.width = 20.0;
    local_rect.height = 20.0;
    check(layout_result_fragment_rect_to_root(
              result, absolute_fragment, local_rect,
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &root_rect)
              == LXB_STATUS_OK,
          "fragment-bound oof local rect maps to root");
    check(root_rect.x == 110.0 && root_rect.y == 10.0
              && root_rect.width == 20.0 && root_rect.height == 20.0,
          "fragment-bound oof rect uses exact containing fragment");
}

static void
hit_order_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *back = make_object(layout);
    layout_object_t *front = make_object(layout);
    layout_object_t *high_z = make_object(layout);
    layout_object_t *same_a = make_object(layout);
    layout_object_t *same_b = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 200.0,
                                                     200.0);
    layout_fragment_t *back_fragment = make_fragment(result, back, 80.0,
                                                     80.0);
    layout_fragment_t *front_fragment = make_fragment(result, front, 80.0,
                                                      80.0);
    layout_fragment_t *high_z_fragment =
        make_fragment_with_stacking_order(result, high_z, 80.0, 80.0, 5);
    layout_fragment_t *same_a_fragment =
        make_fragment_with_stacking_order(result, same_a, 40.0, 40.0, 1);
    layout_fragment_t *same_b_fragment =
        make_fragment_with_stacking_order(result, same_b, 40.0, 40.0, 1);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(back, root) == LXB_STATUS_OK,
          "set back object parent");
    check(layout_object_set_parent(front, root) == LXB_STATUS_OK,
          "set front object parent");
    check(layout_object_set_parent(high_z, root) == LXB_STATUS_OK,
          "set high z object parent");
    check(layout_object_set_parent(same_a, root) == LXB_STATUS_OK,
          "set same-order first object parent");
    check(layout_object_set_parent(same_b, root) == LXB_STATUS_OK,
          "set same-order second object parent");
    check(root_fragment != NULL && back_fragment != NULL
              && front_fragment != NULL && high_z_fragment != NULL
              && same_a_fragment != NULL && same_b_fragment != NULL,
          "hit order fragments allocate");
    check(layout_fragment_append_child(root_fragment, high_z_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "append high stacking fragment first");
    check(layout_fragment_append_child(root_fragment, back_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "append back fragment");
    check(layout_fragment_append_child(root_fragment, front_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "append front fragment");
    check(layout_fragment_append_child(root_fragment, same_a_fragment,
                                       make_point(120.0, 20.0))
              == LXB_STATUS_OK,
          "append same-order first fragment");
    check(layout_fragment_append_child(root_fragment, same_b_fragment,
                                       make_point(120.0, 20.0))
              == LXB_STATUS_OK,
          "append same-order second fragment");
    check(layout_fragment_stacking_order(high_z_fragment) == 5,
          "fragment exposes stacking order");
    check(layout_fragment_link_stacking_order(
              layout_fragment_child_link_at(root_fragment, 0))
              == 5,
          "fragment link stores placement stacking order");
    check(layout_fragment_link_stacking_order(
              layout_fragment_child_link_at(root_fragment, 3))
              == 1,
          "same-order fragment link stores placement stacking order");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "hit order smoke stores result root");
    freeze_result(result);

    check(layout_hit_test(result, root_fragment, make_point(40.0, 40.0),
                          &hit)
              == LXB_STATUS_OK,
          "overlapping fragments hit");
    check(hit == high_z_fragment,
          "hit test prefers higher stacking order over append order");
    check(layout_hit_test(result, root_fragment, make_point(130.0, 30.0),
                          &hit)
              == LXB_STATUS_OK,
          "same-order overlapping fragments hit");
    check(hit == same_b_fragment,
          "same stacking order falls back to reverse physical fragment order");
}

static void
hit_order_paint_phase_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *float_object = make_object(layout);
    layout_object_t *normal_over_float = make_object(layout);
    layout_object_t *normal_over_negative = make_object(layout);
    layout_object_t *negative = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     80.0);
    layout_fragment_t *float_fragment = make_fragment(result, float_object,
                                                      50.0, 50.0);
    layout_fragment_t *normal_float_fragment =
        make_fragment(result, normal_over_float, 50.0, 50.0);
    layout_fragment_t *normal_negative_fragment =
        make_fragment(result, normal_over_negative, 50.0, 50.0);
    layout_fragment_t *negative_fragment =
        make_fragment_with_stacking_order(result, negative, 50.0, 50.0, -1);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(float_object, root) == LXB_STATUS_OK,
          "paint phase float parent set");
    check(layout_object_set_parent(normal_over_float, root) == LXB_STATUS_OK,
          "paint phase normal parent set");
    check(layout_object_set_parent(normal_over_negative, root) == LXB_STATUS_OK,
          "paint phase negative peer normal parent set");
    check(layout_object_set_parent(negative, root) == LXB_STATUS_OK,
          "paint phase negative parent set");
    check(root_fragment != NULL && float_fragment != NULL
              && normal_float_fragment != NULL
              && normal_negative_fragment != NULL
              && negative_fragment != NULL,
          "paint phase fragments allocate");

    check(layout_fragment_append_child_with_placement(
              root_fragment, float_fragment, make_point(10.0, 10.0),
              LAYOUT_FRAGMENT_PLACEMENT_FLOAT, 0)
              == LXB_STATUS_OK,
          "paint phase appends float before normal flow");
    check(layout_fragment_append_child(root_fragment, normal_float_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "paint phase appends overlapping normal after float");
    check(layout_fragment_append_child(root_fragment, normal_negative_fragment,
                                       make_point(90.0, 10.0))
              == LXB_STATUS_OK,
          "paint phase appends normal before negative z");
    check(layout_fragment_append_child(root_fragment, negative_fragment,
                                       make_point(90.0, 10.0))
              == LXB_STATUS_OK,
          "paint phase appends negative z after normal");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "paint phase stores result root");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(20.0, 20.0), &hit)
              == LXB_STATUS_OK,
          "paint phase overlapping float hit");
    check(hit == float_fragment,
          "float placement paints above normal flow despite append order");
    check(layout_hit_test(result, NULL, make_point(100.0, 20.0), &hit)
              == LXB_STATUS_OK,
          "paint phase overlapping negative z hit");
    check(hit == normal_negative_fragment,
          "negative z paints behind normal flow despite append order");
}

static void
hit_order_out_of_flow_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *normal = make_object(layout);
    layout_object_t *low_oof = make_object(layout);
    layout_object_t *same_oof = make_object(layout);
    layout_object_t *high_oof = make_object(layout);
    layout_object_t *first_tie_oof = make_object(layout);
    layout_object_t *last_tie_oof = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     100.0);
    layout_fragment_t *normal_fragment = make_fragment_with_stacking_order(
        result, normal, 60.0, 60.0, 1);
    layout_fragment_t *low_oof_fragment = make_fragment_with_stacking_order(
        result, low_oof, 60.0, 60.0, 0);
    layout_fragment_t *same_oof_fragment = make_fragment_with_stacking_order(
        result, same_oof, 30.0, 30.0, 1);
    layout_fragment_t *high_oof_fragment = make_fragment_with_stacking_order(
        result, high_oof, 60.0, 60.0, 5);
    layout_fragment_t *first_tie_oof_fragment =
        make_fragment_with_stacking_order(result, first_tie_oof, 30.0, 30.0,
                                          2);
    layout_fragment_t *last_tie_oof_fragment =
        make_fragment_with_stacking_order(result, last_tie_oof, 30.0, 30.0,
                                          2);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(normal, root) == LXB_STATUS_OK,
          "oof hit order normal parent set");
    check(layout_object_set_parent(low_oof, root) == LXB_STATUS_OK,
          "oof hit order low oof parent set");
    check(layout_object_set_parent(same_oof, root) == LXB_STATUS_OK,
          "oof hit order same oof parent set");
    check(layout_object_set_parent(high_oof, root) == LXB_STATUS_OK,
          "oof hit order high oof parent set");
    check(layout_object_set_parent(first_tie_oof, root) == LXB_STATUS_OK,
          "oof hit order first tie oof parent set");
    check(layout_object_set_parent(last_tie_oof, root) == LXB_STATUS_OK,
          "oof hit order last tie oof parent set");
    layout_object_set_can_contain_absolute_position(root, true);
    set_position(low_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_position(same_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_position(high_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_position(first_tie_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_position(last_tie_oof, LXB_CSS_POSITION_ABSOLUTE);

    check(root_fragment != NULL && normal_fragment != NULL
              && low_oof_fragment != NULL && same_oof_fragment != NULL
              && high_oof_fragment != NULL
              && first_tie_oof_fragment != NULL
              && last_tie_oof_fragment != NULL,
          "oof hit order fragments allocate");
    check(layout_fragment_append_child(root_fragment, normal_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "oof hit order appends normal-flow fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "oof hit order stores result root");
    check(layout_result_add_out_of_flow_positioned(result, low_oof, root,
                                                   low_oof_fragment,
                                                   make_point(20.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "low z oof registers under containing block");
    check(layout_result_add_out_of_flow_positioned(result, same_oof, root,
                                                   same_oof_fragment,
                                                   make_point(20.0, 50.0),
                                                   false)
              == LXB_STATUS_OK,
          "same z oof registers under containing block");
    check(layout_result_add_out_of_flow_positioned(result, high_oof, root,
                                                   high_oof_fragment,
                                                   make_point(50.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "high z oof registers under containing block");
    check(layout_result_add_out_of_flow_positioned(result, first_tie_oof,
                                                   root,
                                                   first_tie_oof_fragment,
                                                   make_point(120.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "first tie z oof registers under containing block");
    check(layout_result_add_out_of_flow_positioned(result, last_tie_oof,
                                                   root,
                                                   last_tie_oof_fragment,
                                                   make_point(120.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "last tie z oof registers under containing block");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(25.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "normal-flow fragment competes with lower z oof");
    check(hit == normal_fragment,
          "low z oof does not globally cover normal-flow fragment");
    check(layout_hit_test(result, NULL, make_point(25.0, 55.0), &hit)
              == LXB_STATUS_OK,
          "same z oof competes with normal-flow fragment");
    check(hit == same_oof_fragment,
          "same z positioned fragment is visited after normal-flow fragment");
    check(layout_hit_test(result, NULL, make_point(55.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "high z oof overlaps normal-flow fragment");
    check(hit == high_oof_fragment,
          "high z oof still hits above containing block contents");
    check(layout_hit_test(result, NULL, make_point(125.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "same z out-of-flow fragments compete by visual sequence");
    check(hit == last_tie_oof_fragment,
          "later same z out-of-flow fragment is visited first");
}

static void
hit_nested_stacking_context_oof_smoke(layout_t *layout,
                                      layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *stacking = make_object(layout);
    layout_object_t *sibling = make_object(layout);
    layout_object_t *high_oof = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     120.0);
    layout_fragment_t *stacking_fragment = make_fragment(result, stacking,
                                                         90.0, 90.0);
    layout_fragment_t *sibling_fragment = make_fragment(result, sibling,
                                                        50.0, 50.0);
    layout_fragment_t *high_oof_fragment =
        make_fragment_with_stacking_order(result, high_oof, 70.0, 70.0,
                                          10);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(stacking, root) == LXB_STATUS_OK,
          "nested stacking oof stacking parent set");
    check(layout_object_set_parent(sibling, root) == LXB_STATUS_OK,
          "nested stacking oof sibling parent set");
    check(layout_object_set_parent(high_oof, stacking) == LXB_STATUS_OK,
          "nested stacking oof child parent set");
    layout_object_set_can_contain_absolute_position(root, true);
    set_opacity(stacking, 0.5);
    set_position(sibling, LXB_CSS_POSITION_RELATIVE);
    set_z_index(sibling, 1);
    set_position(high_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_z_index(high_oof, 10);

    check(root_fragment != NULL && stacking_fragment != NULL
              && sibling_fragment != NULL && high_oof_fragment != NULL,
          "nested stacking oof fragments allocate");
    check(layout_fragment_append_child(root_fragment, stacking_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "nested stacking oof appends stacking context");
    check(layout_fragment_append_child(root_fragment, sibling_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "nested stacking oof appends sibling");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "nested stacking oof stores result root");
    check(layout_result_add_out_of_flow_positioned(result, high_oof, root,
                                                   high_oof_fragment,
                                                   make_point(20.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "nested stacking oof registers absolute under root geometry");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(30.0, 30.0), &hit)
              == LXB_STATUS_OK,
          "nested stacking oof overlapping sibling hit");
    check(hit == sibling_fragment,
          "ancestor stacking context paints atomically below higher sibling");
    check(layout_hit_test(result, NULL, make_point(75.0, 75.0), &hit)
              == LXB_STATUS_OK,
          "nested stacking oof private area hit");
    check(hit == high_oof_fragment,
          "out-of-flow descendant still hits inside its stacking context");
}

static void
hit_nested_out_of_flow_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *container = make_object(layout);
    layout_object_t *absolute_child = make_object(layout);
    layout_object_t *clipped_container = make_object(layout);
    layout_object_t *clipped_absolute = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 200.0,
                                                     120.0);
    layout_fragment_t *container_fragment = make_fragment(result, container,
                                                          40.0, 40.0);
    layout_fragment_t *absolute_fragment = make_fragment_with_stacking_order(
        result, absolute_child, 30.0, 30.0, 3);
    layout_fragment_t *clipped_container_fragment = make_fragment(
        result, clipped_container, 40.0, 40.0);
    layout_fragment_t *clipped_absolute_fragment =
        make_fragment_with_stacking_order(result, clipped_absolute, 30.0,
                                          30.0, 4);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(container, root) == LXB_STATUS_OK,
          "nested oof container parent set");
    check(layout_object_set_parent(absolute_child, container) == LXB_STATUS_OK,
          "nested oof child parent set");
    check(layout_object_set_parent(clipped_container, root) == LXB_STATUS_OK,
          "nested clipped oof container parent set");
    check(layout_object_set_parent(clipped_absolute, clipped_container)
              == LXB_STATUS_OK,
          "nested clipped oof child parent set");
    layout_object_set_can_contain_absolute_position(container, true);
    layout_object_set_can_contain_absolute_position(clipped_container, true);
    set_position(absolute_child, LXB_CSS_POSITION_ABSOLUTE);
    set_position(clipped_absolute, LXB_CSS_POSITION_ABSOLUTE);
    set_overflow(clipped_container, LXB_CSS_OVERFLOW_X_HIDDEN,
                 LXB_CSS_OVERFLOW_Y_HIDDEN);

    check(root_fragment != NULL && container_fragment != NULL
              && absolute_fragment != NULL
              && clipped_container_fragment != NULL
              && clipped_absolute_fragment != NULL,
          "nested oof fragments allocate");
    check(layout_fragment_append_child(root_fragment, container_fragment,
                                       make_point(60.0, 20.0))
              == LXB_STATUS_OK,
          "nested oof appends containing block fragment");
    check(layout_fragment_append_child(root_fragment,
                                       clipped_container_fragment,
                                       make_point(60.0, 70.0))
              == LXB_STATUS_OK,
          "nested oof appends clipped containing block fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "nested oof stores result root");
    check(layout_result_add_out_of_flow_positioned(result, absolute_child,
                                                   container,
                                                   absolute_fragment,
                                                   make_point(20.0, 10.0),
                                                   false)
              == LXB_STATUS_OK,
          "nested oof registers under nested containing block");
    check(layout_result_add_out_of_flow_positioned(result, clipped_absolute,
                                                   clipped_container,
                                                   clipped_absolute_fragment,
                                                   make_point(20.0, 10.0),
                                                   false)
              == LXB_STATUS_OK,
          "nested clipped oof registers under clipped containing block");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(85.0, 35.0), &hit)
              == LXB_STATUS_OK,
          "nested oof hits inside containing block border box");
    check(hit == absolute_fragment,
          "nested containing block dispatches to positioned fragment");
    check(layout_hit_test(result, NULL, make_point(105.0, 35.0), &hit)
              == LXB_STATUS_OK,
          "nested oof may intersect outside overflow-visible containing block");
    check(hit == absolute_fragment,
          "overflow-visible containing block does not clip positioned child");
    check(layout_hit_test(result, NULL, make_point(105.0, 85.0), &hit)
              == LXB_STATUS_OK,
          "nested clipped oof outside point may still hit root");
    check(hit != clipped_absolute_fragment,
          "overflow-hidden containing block clips positioned child hit");
}

static void
hit_may_intersect_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *zero_height = make_object(layout);
    layout_object_t *floating = make_object(layout);
    layout_object_t *overflow_container = make_object(layout);
    layout_object_t *overflow_child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 180.0,
                                                     140.0);
    layout_fragment_t *zero_height_fragment = make_fragment(result,
                                                           zero_height,
                                                           60.0, 0.0);
    layout_fragment_t *floating_fragment = make_fragment_with_box_type(
        result, floating, 60.0, 60.0, LAYOUT_FRAGMENT_BOX_FLOATING);
    layout_fragment_t *overflow_fragment = make_fragment(result,
                                                         overflow_container,
                                                         10.0, 10.0);
    layout_fragment_t *overflow_child_fragment = make_fragment(
        result, overflow_child, 20.0, 20.0);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(zero_height, root) == LXB_STATUS_OK,
          "hit may-intersect zero-height parent set");
    check(layout_object_set_parent(floating, root) == LXB_STATUS_OK,
          "hit may-intersect floating parent set");
    check(layout_object_set_parent(overflow_container, root)
              == LXB_STATUS_OK,
          "hit may-intersect overflow container parent set");
    check(layout_object_set_parent(overflow_child, overflow_container)
              == LXB_STATUS_OK,
          "hit may-intersect overflow child parent set");
    check(root_fragment != NULL && zero_height_fragment != NULL
              && floating_fragment != NULL && overflow_fragment != NULL
              && overflow_child_fragment != NULL,
          "hit may-intersect fragments allocate");

    check(layout_fragment_append_child(root_fragment, zero_height_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "append zero-height hit test fragment");
    check(layout_fragment_append_child(root_fragment, floating_fragment,
                                       make_point(10.0, 20.0))
              == LXB_STATUS_OK,
          "append floating hit test fragment");
    check(layout_fragment_append_child(root_fragment, overflow_fragment,
                                       make_point(100.0, 90.0))
              == LXB_STATUS_OK,
          "append overflow-visible hit test container");
    check(layout_fragment_append_child(overflow_fragment,
                                       overflow_child_fragment,
                                       make_point(15.0, 15.0))
              == LXB_STATUS_OK,
          "append overflow-visible hit test child");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "hit may-intersect stores result root");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(15.0, 10.0), &hit)
              == LXB_STATUS_OK,
          "zero-height empty leaf falls through to root");
    check(hit == root_fragment, "zero-height empty leaf cannot hit");
    check(layout_hit_test(result, NULL, make_point(20.0, 30.0), &hit)
              == LXB_STATUS_OK,
          "floating visual fragment can still be hit");
    check(hit == floating_fragment,
          "fragment candidate filter is not used for visual hit target");
    check(layout_hit_test(result, NULL, make_point(116.0, 106.0), &hit)
              == LXB_STATUS_OK,
          "may-intersect allows overflow-visible descendants");
    check(hit == overflow_child_fragment,
          "overflow-visible child can hit outside parent border box");
}

static void
physical_fragment_link_offset_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *first = make_object(layout);
    layout_object_t *second = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     80.0);
    layout_fragment_t *first_fragment = make_fragment(result, first, 20.0,
                                                      20.0);
    layout_fragment_t *second_fragment = make_fragment(result, second, 20.0,
                                                       20.0);
    layout_point_t first_offset;
    layout_point_t second_offset;
    layout_point_t first_link_offset;
    layout_point_t second_link_offset;
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(first, root) == LXB_STATUS_OK,
          "link offset set first child parent");
    check(layout_object_set_parent(second, root) == LXB_STATUS_OK,
          "link offset set second child parent");
    check(root_fragment != NULL && first_fragment != NULL
              && second_fragment != NULL,
          "link offset fragments allocate");
    check(layout_fragment_append_child(root_fragment, first_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "first link places child fragment");
    check(layout_fragment_append_child(root_fragment, second_fragment,
                                       make_point(40.0, 10.0))
              == LXB_STATUS_OK,
          "second link places child fragment at independent offset");
    check(layout_fragment_append_child(root_fragment, second_fragment,
                                       make_point(70.0, 10.0))
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "fragment tree keeps a unique path for coordinate queries");
    check(layout_fragment_child_count(root_fragment) == 2,
          "fragment links store child placement");
    first_link_offset = layout_fragment_link_offset(
        layout_fragment_child_link_at(root_fragment, 0));
    second_link_offset = layout_fragment_link_offset(
        layout_fragment_child_link_at(root_fragment, 1));
    check(first_link_offset.x == 10.0 && first_link_offset.y == 10.0
              && second_link_offset.x == 40.0
              && second_link_offset.y == 10.0,
          "fragment link directly exposes physical placement offset");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "link offset smoke stores result root");
    freeze_result(result);

    check(layout_result_fragment_link_offset(result, root_fragment, 0,
                                             &first_offset)
              == LXB_STATUS_OK,
          "link offset exposes first link offset");
    check(layout_result_fragment_link_offset(result, root_fragment, 1,
                                             &second_offset)
              == LXB_STATUS_OK,
          "link offset exposes second link offset");
    check(first_offset.x == 10.0 && first_offset.y == 10.0
              && second_offset.x == 40.0 && second_offset.y == 10.0,
          "link offsets hold physical placement outside child fragments");
    check(layout_hit_test(result, NULL, make_point(45.0, 15.0), &hit)
              == LXB_STATUS_OK,
          "hit test traverses physical fragment link offset");
    check(hit == second_fragment, "hit reaches second linked fragment");
}

static void
physical_fragment_retained_identity_smoke(layout_t *layout,
                                          layout_result_t *result)
{
    layout_result_t *next_result = layout_result_create(layout);
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_object_t *embed_host = make_object(layout);
    layout_fragment_t *root_fragment;
    layout_fragment_t *first_child_fragment;
    layout_fragment_t *second_child_fragment;
    layout_fragment_t *embed_fragment;
    layout_fragment_t *next_child_fragment;
    layout_fragment_link_t *first_link;
    layout_fragment_link_t *second_link;
    layout_fragment_link_t *embed_link;
    layout_fragment_key_t root_key;
    layout_fragment_key_t first_key;
    layout_fragment_key_t second_key;
    layout_fragment_init_t init;

    check(next_result != NULL, "next retained identity result allocates");
    check(layout_result_generation(result) != 0,
          "layout result has nonzero generation");
    check(next_result == NULL
              || layout_result_generation(next_result)
                     != layout_result_generation(result),
          "layout result generations distinguish scene-plan snapshots");
    check(root != NULL && child != NULL && embed_host != NULL,
          "retained identity objects allocate");
    check(layout_object_id(root) != 0 && layout_object_id(child) != 0
              && layout_object_id(root) != layout_object_id(child),
          "layout objects expose stable nonzero ids");

    layout_object_set_native_embed_token(embed_host, 777);

    root_fragment = make_fragment(result, root, 160.0, 100.0);
    first_child_fragment = make_fragment(result, child, 20.0, 20.0);
    second_child_fragment = make_fragment(result, child, 24.0, 18.0);
    embed_fragment = make_fragment(result, embed_host, 40.0, 20.0);

    check(root_fragment != NULL && first_child_fragment != NULL
              && second_child_fragment != NULL && embed_fragment != NULL,
          "retained identity fragments allocate");

    root_key = layout_fragment_key(root_fragment);
    first_key = layout_fragment_key(first_child_fragment);
    second_key = layout_fragment_key(second_child_fragment);

    check(root_key.object_id == layout_object_id(root),
          "fragment key carries layout object id");
    check(root_key.fragment_index == 0,
          "first fragment for an object uses first fragment index");
    check(first_key.object_id == layout_object_id(child)
              && second_key.object_id == layout_object_id(child),
          "same object fragments keep object identity in key");
    check(first_key.fragment_index != second_key.fragment_index,
          "same object multiple fragments get distinct fragment indexes");

    next_child_fragment = make_fragment(next_result, child, 20.0, 20.0);
    check(next_child_fragment != NULL
              && layout_fragment_key(next_child_fragment).object_id
                     == first_key.object_id
              && layout_fragment_key(next_child_fragment).fragment_index
                     == first_key.fragment_index,
          "same object fragment ordinal is stable across layout results");
    check(layout_fragment_role(root_fragment) == LAYOUT_FRAGMENT_ROLE_ANONYMOUS
              && root_key.role == LAYOUT_FRAGMENT_ROLE_ANONYMOUS,
          "anonymous object fragments get anonymous role by default");
    check(layout_fragment_role(embed_fragment)
              == LAYOUT_FRAGMENT_ROLE_NATIVE_EMBED,
          "native embed host fragment gets native embed role by default");

    check(layout_fragment_append_child(root_fragment, first_child_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "retained identity appends first child");
    check(layout_fragment_append_child_with_placement(
              root_fragment, second_child_fragment, make_point(40.0, 10.0),
              LAYOUT_FRAGMENT_PLACEMENT_FLOAT, 0)
              == LXB_STATUS_OK,
          "retained identity appends child with placement token");
    check(layout_fragment_append_child(root_fragment, embed_fragment,
                                       make_point(70.0, 10.0))
              == LXB_STATUS_OK,
          "retained identity appends native embed child");

    first_link = layout_fragment_child_link_at(root_fragment, 0);
    second_link = layout_fragment_child_link_at(root_fragment, 1);
    embed_link = layout_fragment_child_link_at(root_fragment, 2);

    check(layout_fragment_link_sequence(first_link) == 0
              && layout_fragment_link_sequence(second_link) == 1
              && layout_fragment_link_sequence(embed_link) == 2,
          "fragment links expose stable parent sequence slots");
    check(layout_fragment_link_placement(first_link)
              == LAYOUT_FRAGMENT_PLACEMENT_NORMAL,
          "default fragment link placement is normal");
    check(layout_fragment_link_placement(second_link)
              == LAYOUT_FRAGMENT_PLACEMENT_FLOAT,
          "fragment link exposes explicit placement");

    memset(&init, 0, sizeof(init));
    init.object = child;
    init.size.width = 12.0;
    init.size.height = 12.0;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    init.role = LAYOUT_FRAGMENT_ROLE_PSEUDO;
    second_child_fragment = layout_fragment_create(result, &init);
    second_key = layout_fragment_key(second_child_fragment);
    check(second_child_fragment != NULL
              && layout_fragment_role(second_child_fragment)
                     == LAYOUT_FRAGMENT_ROLE_PSEUDO
              && second_key.role == LAYOUT_FRAGMENT_ROLE_PSEUDO,
          "fragment init can override role for pseudo/future fragments");

    layout_result_destroy(next_result, true);
}

static void
physical_fragment_subtree_reuse_smoke(layout_t *layout,
                                      layout_result_t *result)
{
    layout_result_t *clone_result = layout_result_create(layout);
    layout_result_t *reuse_result = layout_result_create(layout);
    layout_object_t *source_parent = make_object(layout);
    layout_object_t *source_root = make_object(layout);
    layout_object_t *source_child = make_object(layout);
    layout_object_t *target_root = make_object(layout);
    layout_fragment_t *source_parent_fragment = make_fragment(result,
                                                             source_parent,
                                                             120.0, 100.0);
    layout_fragment_t *source_root_fragment = make_fragment(result,
                                                           source_root,
                                                           80.0, 80.0);
    layout_fragment_t *source_child_fragment = make_fragment(result,
                                                            source_child,
                                                            20.0, 20.0);
    layout_fragment_t *target_root_fragment;
    layout_fragment_t *cloned_root_fragment;
    layout_fragment_t *cloned_child_fragment;
    layout_fragment_t *placed_root_fragment;
    layout_fragment_t *placed_child_fragment;
    layout_fragment_link_t *source_root_link;
    layout_fragment_link_t *cloned_child_link;
    layout_point_t cloned_child_offset;
    layout_fragment_t *hit = NULL;
    layout_result_t *invalid_source_target = NULL;

    check(clone_result != NULL, "fragment clone result allocates");
    check(reuse_result != NULL, "fragment reuse result allocates");
    check(layout_object_set_parent(source_root, source_parent)
              == LXB_STATUS_OK,
          "fragment reuse source root parent is set");
    check(layout_object_set_parent(source_child, source_root)
              == LXB_STATUS_OK,
          "fragment reuse source object parent is set");
    check(source_parent_fragment != NULL && source_root_fragment != NULL
              && source_child_fragment != NULL,
          "fragment reuse source fragments allocate");
    check(layout_fragment_append_child(source_parent_fragment,
                                       source_root_fragment,
                                       make_point(3.0, 5.0))
              == LXB_STATUS_OK,
          "fragment reuse source root is placed by parent link");
    check(layout_fragment_append_child(source_root_fragment,
                                       source_child_fragment,
                                       make_point(12.0, 14.0))
              == LXB_STATUS_OK,
          "fragment reuse source link stores child placement");
    check(layout_result_set_root_fragment(result, source_parent_fragment)
              == LXB_STATUS_OK,
          "fragment reuse source result stores root");
    check(layout_fragment_clone_subtree(clone_result, source_root_fragment)
              == NULL,
          "fragment subtree clone rejects unfinished source result");
    freeze_result(result);

    cloned_root_fragment =
        layout_fragment_clone_subtree(clone_result, source_root_fragment);
    check(cloned_root_fragment != NULL,
          "fragment subtree clones into a new result");
    check(cloned_root_fragment != source_root_fragment,
          "fragment subtree clone has independent root fragment");
    check(layout_fragment_object(cloned_root_fragment) == source_root,
          "fragment subtree clone preserves layout object identity");
    check(layout_fragment_child_count(cloned_root_fragment) == 1,
          "fragment subtree clone preserves child links");

    cloned_child_link = layout_fragment_first_child_link(cloned_root_fragment);
    cloned_child_fragment = layout_fragment_link_fragment(cloned_child_link);
    cloned_child_offset = layout_fragment_link_offset(cloned_child_link);
    check(cloned_child_fragment != NULL
              && cloned_child_fragment != source_child_fragment,
          "fragment subtree clone creates independent child fragment");
    check(layout_fragment_object(cloned_child_fragment) == source_child,
          "fragment subtree clone preserves child object identity");
    check(cloned_child_offset.x == 12.0 && cloned_child_offset.y == 14.0,
          "fragment subtree clone preserves child placement on link");
    check(layout_result_set_root_fragment(clone_result, cloned_root_fragment)
              == LXB_STATUS_OK,
          "fragment clone result stores cloned root");
    freeze_result(clone_result);

    target_root_fragment = make_fragment(reuse_result, target_root, 200.0,
                                         120.0);
    check(target_root_fragment != NULL,
          "fragment reuse target root allocates");
    source_root_link = layout_fragment_first_child_link(target_root_fragment);
    check(source_root_link == NULL,
          "fragment reuse target starts without child links");
    source_root_link = layout_fragment_first_child_link(source_parent_fragment);
    check(source_root_link != NULL,
          "fragment reuse source exposes child link for link cloning");
    check(layout_fragment_append_cloned_child(target_root_fragment,
                                             source_root_link,
                                             make_point(70.0, 10.0),
                                             &placed_root_fragment)
              == LXB_STATUS_OK,
          "cloned subtree is placed by cloning a source fragment link");
    check(layout_fragment_append_cloned_child(target_root_fragment, NULL,
                                             make_point(0.0, 0.0), NULL)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "fragment link clone rejects null source link");
    check(layout_result_set_root_fragment(reuse_result, target_root_fragment)
              == LXB_STATUS_OK,
          "fragment reuse target result stores root");
    freeze_result(reuse_result);

    check(layout_result_fragment_link_offset(reuse_result,
                                             placed_root_fragment, 0,
                                             &cloned_child_offset)
              == LXB_STATUS_OK,
          "fragment subtree clone exposes preserved child link offset");
    check(cloned_child_offset.x == 12.0 && cloned_child_offset.y == 14.0,
          "fragment subtree clone keeps child placement in links");
    check(layout_hit_test(reuse_result, NULL, make_point(83.0, 25.0), &hit)
              == LXB_STATUS_OK,
          "reused fragment subtree hit test uses new parent placement");
    placed_child_fragment = layout_fragment_link_fragment(
        layout_fragment_first_child_link(placed_root_fragment));
    check(hit == placed_child_fragment,
          "reused fragment subtree reaches cloned child fragment");
    check(layout_hit_test(result, NULL, make_point(16.0, 20.0), &hit)
              == LXB_STATUS_OK,
          "source fragment subtree remains usable after clone");
    check(hit == source_child_fragment,
          "source fragment subtree keeps original child fragment");
    check(layout_fragment_clone_subtree(clone_result, source_root_fragment)
              == NULL,
          "fragment subtree clone rejects frozen target result");
    placed_root_fragment = source_root_fragment;
    check(layout_fragment_append_cloned_child(target_root_fragment,
                                             source_root_link,
                                             make_point(0.0, 0.0),
                                             &placed_root_fragment)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "fragment link clone rejects frozen target result");
    check(placed_root_fragment == NULL,
          "failed fragment link clone clears output fragment");

    layout_fragment_invalidate_children(source_root_fragment);
    invalid_source_target = layout_result_create(layout);
    check(invalid_source_target != NULL,
          "fragment reuse invalid-source target result allocates");
    check(layout_fragment_clone_subtree(invalid_source_target,
                                       source_root_fragment) == NULL,
          "fragment subtree clone rejects invalid source children");

    layout_result_destroy(invalid_source_target, true);
    layout_result_destroy(clone_result, true);
    layout_result_destroy(reuse_result, true);
}

static void
physical_fragment_result_freeze_smoke(layout_t *layout,
                                      layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_object_t *late_child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 100.0,
                                                     100.0);
    layout_fragment_t *child_fragment = make_fragment(result, child, 20.0,
                                                      20.0);
    layout_fragment_t *late_child_fragment = make_fragment(result, late_child,
                                                           10.0, 10.0);
    layout_fragment_init_t late_init;

    check(layout_object_set_parent(child, root) == LXB_STATUS_OK,
          "freeze smoke set child parent");
    check(layout_object_set_parent(late_child, root) == LXB_STATUS_OK,
          "freeze smoke set late child parent");
    check(root_fragment != NULL && child_fragment != NULL
              && late_child_fragment != NULL,
          "freeze smoke fragments allocate before result freeze");
    check(layout_fragment_append_child(root_fragment, child_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "freeze smoke appends initial child link");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "freeze smoke stores result root");
    freeze_result(result);

    memset(&late_init, 0, sizeof(late_init));
    late_init.object = late_child;
    late_init.size.width = 5.0;
    late_init.size.height = 5.0;
    late_init.type = LAYOUT_FRAGMENT_BOX;
    late_init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    check(layout_fragment_create(result, &late_init) == NULL,
          "frozen result rejects new physical fragment allocation");
    check(layout_fragment_append_child(root_fragment, late_child_fragment,
                                       make_point(40.0, 10.0))
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "frozen result rejects new child link");
    check(layout_result_set_root_fragment(result, child_fragment)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "frozen result rejects root fragment replacement");
}

static void
physical_fragment_traversal_guard_smoke(layout_t *layout,
                                        layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 100.0,
                                                     100.0);
    layout_fragment_t *child_fragment = make_fragment(result, child, 20.0,
                                                      20.0);
    layout_fragment_t *hit = NULL;
    layout_point_t local = make_point(0.0, 0.0);

    check(layout_object_set_parent(child, root) == LXB_STATUS_OK,
          "traversal guard set child parent");
    check(root_fragment != NULL && child_fragment != NULL,
          "traversal guard fragments allocate");
    check(layout_fragment_append_child(root_fragment, child_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "traversal guard appends child fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "traversal guard stores result root");
    freeze_result(result);

    layout_object_set_can_traverse_physical_fragments(child, false);
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "hit test rejects non-traversable physical fragment tree");
    check(hit == NULL, "non-traversable hit test returns no fragment");
    check(layout_result_point_in_fragment(result, child_fragment,
                                          make_point(15.0, 15.0),
                                          &local)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "coordinate query rejects non-traversable physical fragment path");

    layout_object_set_can_traverse_physical_fragments(child, true);
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_OK,
          "hit test works after physical fragment traversal is allowed");
    check(hit == child_fragment,
          "traversable physical fragment tree reaches child");
    check(layout_fragment_children_valid(root_fragment),
          "physical fragment children are valid before invalidation");
    layout_fragment_invalidate_children(root_fragment);
    check(!layout_fragment_children_valid(root_fragment),
          "physical fragment children invalidation is observable");
    check(layout_fragment_child_count(root_fragment) == 0,
          "invalid physical fragment children report zero count");
    check(layout_fragment_first_child_link(root_fragment) == NULL,
          "invalid physical fragment children expose no first link");
    check(layout_fragment_child_link_at(root_fragment, 0) == NULL,
          "invalid physical fragment children expose no indexed link");
    check(layout_hit_test(result, NULL, make_point(15.0, 15.0), &hit)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "hit test rejects invalid physical fragment children");
    check(hit == NULL, "invalid physical fragment children return no hit");
    check(layout_result_point_in_fragment(result, child_fragment,
                                          make_point(15.0, 15.0),
                                          &local)
              == LXB_STATUS_ERROR_WRONG_STAGE,
          "coordinate query rejects invalid physical fragment children");
}

static void
fragment_paint_flags_hit_test_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *hidden = make_object(layout);
    layout_object_t *opaque = make_object(layout);
    layout_object_t *opaque_child = make_object(layout);
    layout_object_t *pointer_none = make_object(layout);
    layout_object_t *pointer_none_child = make_object(layout);
    layout_object_t *visibility_hidden = make_object(layout);
    layout_object_t *visibility_hidden_child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 200.0,
                                                     200.0);
    layout_fragment_t *hidden_fragment =
        make_fragment_with_flags(result, hidden, 50.0, 50.0,
                                 LAYOUT_FRAGMENT_HIDDEN_FOR_PAINT);
    layout_fragment_t *opaque_fragment =
        make_fragment_with_flags(result, opaque, 80.0, 80.0,
                                 LAYOUT_FRAGMENT_OPAQUE);
    layout_fragment_t *opaque_child_fragment = make_fragment(result,
                                                            opaque_child,
                                                            20.0, 20.0);
    layout_fragment_t *pointer_none_fragment =
        make_fragment(result, pointer_none, 40.0, 40.0);
    layout_fragment_t *pointer_none_child_fragment =
        make_fragment(result, pointer_none_child, 10.0, 10.0);
    layout_fragment_t *visibility_hidden_fragment =
        make_fragment(result, visibility_hidden, 40.0, 40.0);
    layout_fragment_t *visibility_hidden_child_fragment =
        make_fragment(result, visibility_hidden_child, 10.0, 10.0);
    layout_fragment_t *hit = NULL;

    check(layout_object_set_parent(hidden, root) == LXB_STATUS_OK,
          "set hidden fragment object parent");
    check(layout_object_set_parent(opaque, root) == LXB_STATUS_OK,
          "set opaque fragment object parent");
    check(layout_object_set_parent(opaque_child, opaque) == LXB_STATUS_OK,
          "set opaque child object parent");
    check(layout_object_set_parent(pointer_none, root) == LXB_STATUS_OK,
          "set pointer-events-none parent object");
    check(layout_object_set_parent(pointer_none_child, pointer_none)
              == LXB_STATUS_OK,
          "set pointer-events-none child object");
    check(layout_object_set_parent(visibility_hidden, root) == LXB_STATUS_OK,
          "set visibility-hidden parent object");
    check(layout_object_set_parent(visibility_hidden_child,
                                   visibility_hidden) == LXB_STATUS_OK,
          "set visibility-hidden child object");
    layout_object_set_pointer_events_none(pointer_none, true);
    set_visibility(visibility_hidden, LXB_CSS_VISIBILITY_HIDDEN);
    set_visibility(visibility_hidden_child, LXB_CSS_VISIBILITY_VISIBLE);
    check(root_fragment != NULL && hidden_fragment != NULL
              && opaque_fragment != NULL && opaque_child_fragment != NULL
              && pointer_none_fragment != NULL
              && pointer_none_child_fragment != NULL
              && visibility_hidden_fragment != NULL
              && visibility_hidden_child_fragment != NULL,
          "paint flag smoke fragments allocate");
    check(layout_fragment_hidden_for_paint(hidden_fragment),
          "hidden fragment exposes paint flag");
    check(layout_fragment_opaque(opaque_fragment),
          "opaque fragment exposes paint flag");
    check((layout_fragment_flags(root_fragment)
           & LAYOUT_FRAGMENT_CHILDREN_VALID) != 0,
          "fragment creation marks children valid");
    check(layout_fragment_append_child(root_fragment, hidden_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "append hidden fragment");
    check(layout_fragment_append_child(root_fragment, opaque_fragment,
                                       make_point(80.0, 10.0))
              == LXB_STATUS_OK,
          "append opaque fragment");
    check(layout_fragment_append_child(opaque_fragment,
                                       opaque_child_fragment,
                                       make_point(5.0, 5.0))
              == LXB_STATUS_OK,
          "append opaque child fragment");
    check(layout_fragment_append_child(root_fragment, pointer_none_fragment,
                                       make_point(10.0, 80.0))
              == LXB_STATUS_OK,
          "append pointer-events-none fragment");
    check(layout_fragment_append_child(pointer_none_fragment,
                                       pointer_none_child_fragment,
                                       make_point(5.0, 5.0))
              == LXB_STATUS_OK,
          "append pointer-events-none child fragment");
    check(layout_fragment_append_child(root_fragment,
                                       visibility_hidden_fragment,
                                       make_point(80.0, 80.0))
              == LXB_STATUS_OK,
          "append visibility-hidden fragment");
    check(layout_fragment_append_child(visibility_hidden_fragment,
                                       visibility_hidden_child_fragment,
                                       make_point(5.0, 5.0))
              == LXB_STATUS_OK,
          "append visibility-hidden child fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "paint flag smoke stores result root");
    freeze_result(result);

    check(layout_hit_test(result, NULL, make_point(20.0, 20.0), &hit)
              == LXB_STATUS_OK,
          "hidden fragment point falls through to root");
    check(hit == root_fragment,
          "hidden-for-paint fragment is skipped by hit test");
    check(layout_hit_test(result, NULL, make_point(90.0, 20.0), &hit)
              == LXB_STATUS_OK,
          "opaque child point hits through opaque parent");
    check(hit == opaque_child_fragment,
          "opaque fragment still allows child hit testing");
    check(layout_hit_test(result, NULL, make_point(140.0, 60.0), &hit)
              == LXB_STATUS_OK,
          "opaque self point falls through to root");
    check(hit == root_fragment,
          "opaque fragment itself is not a hit target");
    check(layout_hit_test(result, NULL, make_point(17.0, 87.0), &hit)
              == LXB_STATUS_OK,
          "pointer-events-none parent still allows child traversal");
    check(hit == pointer_none_child_fragment,
          "pointer-events-none parent does not block hit-testable child");
    check(layout_hit_test(result, NULL, make_point(35.0, 105.0), &hit)
              == LXB_STATUS_OK,
          "pointer-events-none parent self point falls through");
    check(hit == root_fragment,
          "pointer-events-none fragment itself is not a hit target");
    check(layout_hit_test(result, NULL, make_point(87.0, 87.0), &hit)
              == LXB_STATUS_OK,
          "visibility-hidden parent still allows visible child traversal");
    check(hit == visibility_hidden_child_fragment,
          "visibility-visible child can be hit under hidden parent");
    check(layout_hit_test(result, NULL, make_point(105.0, 105.0), &hit)
              == LXB_STATUS_OK,
          "visibility-hidden parent self point falls through");
    check(hit == root_fragment,
          "visibility-hidden fragment itself is not a hit target");
}

static void
fragment_data_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *first = make_object(layout);
    layout_object_t *second = make_object(layout);
    layout_fragment_data_t *first_data;
    layout_fragment_data_t *second_data;
    layout_fragment_t *physical_fragment;
    layout_point_t paint_offset;

    check(first != NULL && second != NULL, "fragment data objects allocate");

    first_data = layout_object_first_fragment_data(first);
    second_data = layout_object_first_fragment_data(second);

    check(layout_object_fragment_data_list(first) != NULL,
          "layout object owns fragment data list metadata");
    check(layout_object_fragment_data_count(first) == 1,
          "layout object starts with one fragment data entry");
    check(first_data != NULL && second_data != NULL,
          "first fragment data entry exists");
    check(layout_fragment_data_is_first(first_data),
          "first fragment data is marked as first");

    layout_fragment_data_set_paint_offset(first_data, make_point(7.0, 11.0));
    paint_offset = layout_fragment_data_paint_offset(first_data);
    check(paint_offset.x == 7.0 && paint_offset.y == 11.0,
          "fragment data stores paint offset metadata");

    physical_fragment = make_fragment(result, first, 20.0, 30.0);
    check(physical_fragment != NULL, "physical fragment allocates");
    check(layout_fragment_object(physical_fragment) == first,
          "physical fragment points back to layout object identity");
    check(layout_object_first_fragment_data(first) == first_data,
          "physical fragment creation does not replace object fragment data");
}

static void
native_embed_host_token_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *embed_host = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 240.0,
                                                     160.0);
    layout_fragment_t *embed_fragment = make_fragment(result, embed_host,
                                                      80.0, 40.0);
    layout_fragment_t *found_fragment = NULL;
    layout_rect_t local_rect;
    layout_rect_t root_rect;

    local_rect.x = 0.0;
    local_rect.y = 0.0;
    local_rect.width = 80.0;
    local_rect.height = 40.0;

    check(layout_object_set_parent(embed_host, root) == LXB_STATUS_OK,
          "native embed host set parent");
    check(root_fragment != NULL && embed_fragment != NULL,
          "native embed host fragments allocate");
    check(!layout_object_is_native_embed_host(embed_host),
          "native embed host token defaults to empty");

    layout_object_set_native_embed_token(embed_host, 42);
    check(layout_object_is_native_embed_host(embed_host),
          "native embed host token marks object");
    check(layout_object_native_embed_token(embed_host) == 42,
          "native embed host token is observable");
    check(layout_object_parent(embed_host) == root,
          "native embed token does not alter layout parent");

    check(layout_fragment_append_child(root_fragment, embed_fragment,
                                       make_point(30.0, 50.0))
              == LXB_STATUS_OK,
          "native embed host fragment remains normal layout output");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "native embed host result stores root");
    freeze_result(result);

    check(layout_result_fragment_for_object(result, embed_host, 0,
                                            &found_fragment)
              == LXB_STATUS_OK,
          "native embed host resolves to physical fragment");
    check(found_fragment == embed_fragment,
          "native embed host fragment lookup returns matching fragment");
    check(layout_result_fragment_rect_to_root(
              result, found_fragment, local_rect,
              LAYOUT_COORDINATE_APPLY_TRANSFORMS, &root_rect)
              == LXB_STATUS_OK,
          "native embed host rect maps to root coordinates");
    check(root_rect.x == 30.0 && root_rect.y == 50.0
              && root_rect.width == 80.0 && root_rect.height == 40.0,
          "native embed host geometry comes from fragment tree");

    layout_object_set_native_embed_token(embed_host, 0);
    check(!layout_object_is_native_embed_host(embed_host),
          "native embed host token clears without tree mutation");
}

static void
layout_scene_plan_build_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *transformed = make_object(layout);
    layout_object_t *clipped = make_object(layout);
    layout_object_t *embed_host = make_object(layout);
    layout_object_t *oof = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 300.0,
                                                     220.0);
    layout_fragment_t *transformed_fragment;
    layout_fragment_t *clipped_fragment = make_fragment(result, clipped,
                                                        90.0, 60.0);
    layout_fragment_t *embed_fragment = make_fragment(result, embed_host,
                                                      40.0, 20.0);
    layout_fragment_t *oof_fragment =
        make_fragment_with_box_type(result, oof, 25.0, 25.0,
                                    LAYOUT_FRAGMENT_BOX_OUT_OF_FLOW_POSITIONED);
    layout_fragment_init_t transformed_init;
    layout_scene_plan_t *plan = NULL;
    const layout_scene_node_t *root_node;
    const layout_scene_node_t *transformed_node;
    const layout_scene_node_t *clipped_node;
    const layout_scene_node_t *embed_node;
    const layout_scene_node_t *oof_node;
    layout_point_t offset;
    layout_rect_t rect;
    unsigned hints;

    memset(&transformed_init, 0, sizeof(transformed_init));
    transformed_init.object = transformed;
    transformed_init.size.width = 50.0;
    transformed_init.size.height = 40.0;
    transformed_init.type = LAYOUT_FRAGMENT_BOX;
    transformed_init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    transformed_init.transform.a = 1.0;
    transformed_init.transform.d = 1.0;
    transformed_init.transform.e = 6.0;
    transformed_init.transform.f = 7.0;

    transformed_fragment = layout_fragment_create(result, &transformed_init);

    check(root != NULL && transformed != NULL && clipped != NULL
              && embed_host != NULL && oof != NULL,
          "scene plan objects allocate");
    check(root_fragment != NULL && transformed_fragment != NULL
              && clipped_fragment != NULL && embed_fragment != NULL
              && oof_fragment != NULL,
          "scene plan fragments allocate");

    set_overflow(clipped, LXB_CSS_OVERFLOW_X_HIDDEN,
                 LXB_CSS_OVERFLOW_Y_VISIBLE);
    set_opacity(transformed, 0.5);
    layout_object_set_native_embed_token(embed_host, 404);
    layout_object_set_can_contain_absolute_position(root, true);
    set_position(oof, LXB_CSS_POSITION_ABSOLUTE);

    check(layout_object_set_parent(transformed, root) == LXB_STATUS_OK,
          "scene plan transformed parent is set");
    check(layout_object_set_parent(clipped, root) == LXB_STATUS_OK,
          "scene plan clipped parent is set");
    check(layout_object_set_parent(embed_host, root) == LXB_STATUS_OK,
          "scene plan embed parent is set");
    check(layout_object_set_parent(oof, root) == LXB_STATUS_OK,
          "scene plan out-of-flow parent is set");
    check(layout_fragment_append_child(root_fragment, transformed_fragment,
                                       make_point(10.0, 12.0))
              == LXB_STATUS_OK,
          "scene plan appends transformed child");
    check(layout_fragment_append_child(root_fragment, clipped_fragment,
                                       make_point(70.0, 12.0))
              == LXB_STATUS_OK,
          "scene plan appends clipped child");
    check(layout_fragment_append_child(root_fragment, embed_fragment,
                                       make_point(120.0, 12.0))
              == LXB_STATUS_OK,
          "scene plan appends native embed child");
    check(layout_result_add_out_of_flow_positioned_in_fragment(
              result, oof, root, root_fragment, oof_fragment,
              make_point(160.0, 30.0), false)
              == LXB_STATUS_OK,
          "scene plan stores out-of-flow fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "scene plan result stores root");
    freeze_result(result);

    plan = layout_scene_plan_create(layout);
    check(plan != NULL, "scene plan creates");
    if (plan == NULL) {
        return;
    }

    check(layout_scene_plan_build(plan, result) == LXB_STATUS_OK,
          "scene plan builds from frozen physical fragments");
    check(layout_scene_plan_generation(plan) == layout_result_generation(result),
          "scene plan carries result generation token");
    check(layout_scene_plan_node_count(plan) == 5,
          "scene plan contains normal children and out-of-flow child");

    root_node = layout_scene_plan_node_at(plan, 0);
    clipped_node = layout_scene_plan_node_at(plan, 1);
    embed_node = layout_scene_plan_node_at(plan, 2);
    transformed_node = layout_scene_plan_node_at(plan, 3);
    oof_node = layout_scene_plan_node_at(plan, 4);

    check(layout_scene_node_parent_index(root_node) == LAYOUT_SCENE_NODE_NONE,
          "scene root has no parent index");
    check(layout_scene_node_child_count(root_node) == 4,
          "scene root sees retained child chain including OOF");
    check(layout_scene_node_first_child_index(root_node) == 1
              && layout_scene_node_last_child_index(root_node) == 4,
          "scene root exposes first and last retained child");
    check(layout_scene_node_previous_sibling_index(clipped_node)
                  == LAYOUT_SCENE_NODE_NONE
              && layout_scene_node_next_sibling_index(clipped_node) == 2,
          "scene plan exposes sibling chain");
    check(layout_scene_node_child_slot(clipped_node) == 0
              && layout_scene_node_child_slot(embed_node) == 1
              && layout_scene_node_child_slot(transformed_node) == 2
              && layout_scene_node_child_slot(oof_node) == 3,
          "scene plan exposes paint-order child slots");
    check(fragment_key_empty(layout_scene_node_parent_key(root_node))
              && fragment_key_equal(layout_scene_node_parent_key(clipped_node),
                                    layout_scene_node_key(root_node))
              && fragment_key_equal(
                     layout_scene_node_previous_sibling_key(transformed_node),
                     layout_scene_node_key(embed_node)),
          "scene plan exposes parent and previous-sibling slot keys");

    offset = layout_scene_node_offset(transformed_node);
    rect = layout_scene_node_local_rect(transformed_node);
    hints = layout_scene_node_layer_hints(transformed_node);
    check(offset.x == 10.0 && offset.y == 12.0
              && rect.width == 50.0 && rect.height == 40.0,
          "scene node carries offset and local rect");
    check((hints & LAYOUT_SCENE_LAYER_HINT_TRANSFORM) != 0
              && (hints & LAYOUT_SCENE_LAYER_HINT_OPACITY) != 0
              && (layout_scene_node_flags(transformed_node)
                  & LAYOUT_SCENE_NODE_STACKING_CONTEXT) != 0
              && layout_scene_node_opacity(transformed_node) == 0.5,
          "scene node carries transform, opacity, and stacking hints");
    check(layout_scene_node_clip_axes(clipped_node) == LAYOUT_OVERFLOW_CLIP_X
              && (layout_scene_node_layer_hints(clipped_node)
                  & LAYOUT_SCENE_LAYER_HINT_CLIP) != 0,
          "scene node carries overflow clip hint");
    check((layout_scene_node_flags(embed_node)
           & LAYOUT_SCENE_NODE_NATIVE_EMBED) != 0
              && layout_scene_node_native_embed_token(embed_node) == 404,
          "scene node carries native embed token and flag");
    check(layout_scene_node_parent_index(oof_node) == 0
              && layout_scene_node_placement(oof_node)
                     == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW
              && layout_scene_node_sequence(oof_node) == 0,
          "scene plan attaches OOF fragment under containing fragment");

    layout_scene_plan_destroy(plan, true);
}

static void
layout_scene_plan_paint_order_oof_smoke(layout_t *layout,
                                        layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *normal = make_object(layout);
    layout_object_t *low_oof = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     100.0);
    layout_fragment_t *normal_fragment =
        make_fragment_with_stacking_order(result, normal, 60.0, 60.0, 1);
    layout_fragment_t *low_oof_fragment =
        make_fragment_with_stacking_order(result, low_oof, 60.0, 60.0, 0);
    layout_scene_plan_t *plan = NULL;
    const layout_scene_node_t *root_node;
    const layout_scene_node_t *oof_node;
    const layout_scene_node_t *normal_node;

    check(layout_object_set_parent(normal, root) == LXB_STATUS_OK,
          "scene paint order normal parent set");
    check(layout_object_set_parent(low_oof, root) == LXB_STATUS_OK,
          "scene paint order oof parent set");
    layout_object_set_can_contain_absolute_position(root, true);
    set_position(low_oof, LXB_CSS_POSITION_ABSOLUTE);
    check(root_fragment != NULL && normal_fragment != NULL
              && low_oof_fragment != NULL,
          "scene paint order fragments allocate");

    check(layout_fragment_append_child(root_fragment, normal_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "scene paint order appends positive z normal child");
    check(layout_result_add_out_of_flow_positioned_in_fragment(
              result, low_oof, root, root_fragment, low_oof_fragment,
              make_point(20.0, 20.0), false)
              == LXB_STATUS_OK,
          "scene paint order registers lower z oof");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "scene paint order stores result root");
    freeze_result(result);

    plan = layout_scene_plan_create(layout);
    check(plan != NULL, "scene paint order plan creates");
    if (plan == NULL) {
        return;
    }

    check(layout_scene_plan_build(plan, result) == LXB_STATUS_OK,
          "scene paint order plan builds");
    check(layout_scene_plan_node_count(plan) == 3,
          "scene paint order includes root, oof, and normal");

    root_node = layout_scene_plan_node_at(plan, 0);
    oof_node = layout_scene_plan_node_at(plan, 1);
    normal_node = layout_scene_plan_node_at(plan, 2);

    check(layout_scene_node_child_count(root_node) == 2,
          "scene paint order root has two retained children");
    check(fragment_key_equal(layout_scene_node_key(oof_node),
                             layout_fragment_key(low_oof_fragment))
              && fragment_key_equal(layout_scene_node_key(normal_node),
                                    layout_fragment_key(normal_fragment)),
          "scene plan orders lower phase oof before positive z normal");
    check(layout_scene_node_child_slot(oof_node) == 0
              && layout_scene_node_child_slot(normal_node) == 1
              && fragment_key_equal(
                     layout_scene_node_previous_sibling_key(normal_node),
                     layout_scene_node_key(oof_node)),
          "scene paint order exposes retained move slot keys");
    check(layout_scene_node_placement(oof_node)
              == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW
              && layout_scene_node_stacking_order(normal_node) == 1,
          "scene paint order preserves placement and stacking metadata");

    layout_scene_plan_destroy(plan, true);
}

static void
layout_scene_plan_nested_stacking_context_oof_smoke(layout_t *layout,
                                                    layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *stacking = make_object(layout);
    layout_object_t *sibling = make_object(layout);
    layout_object_t *high_oof = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 160.0,
                                                     120.0);
    layout_fragment_t *stacking_fragment = make_fragment(result, stacking,
                                                         90.0, 90.0);
    layout_fragment_t *sibling_fragment = make_fragment(result, sibling,
                                                        50.0, 50.0);
    layout_fragment_t *high_oof_fragment =
        make_fragment_with_stacking_order(result, high_oof, 70.0, 70.0,
                                          10);
    layout_scene_plan_t *plan = NULL;
    const layout_scene_node_t *stacking_node = NULL;
    const layout_scene_node_t *sibling_node = NULL;
    const layout_scene_node_t *oof_node = NULL;
    layout_point_t offset;

    check(layout_object_set_parent(stacking, root) == LXB_STATUS_OK,
          "scene nested stacking oof stacking parent set");
    check(layout_object_set_parent(sibling, root) == LXB_STATUS_OK,
          "scene nested stacking oof sibling parent set");
    check(layout_object_set_parent(high_oof, stacking) == LXB_STATUS_OK,
          "scene nested stacking oof child parent set");
    layout_object_set_can_contain_absolute_position(root, true);
    set_opacity(stacking, 0.5);
    set_position(sibling, LXB_CSS_POSITION_RELATIVE);
    set_z_index(sibling, 1);
    set_position(high_oof, LXB_CSS_POSITION_ABSOLUTE);
    set_z_index(high_oof, 10);

    check(root_fragment != NULL && stacking_fragment != NULL
              && sibling_fragment != NULL && high_oof_fragment != NULL,
          "scene nested stacking oof fragments allocate");
    check(layout_fragment_append_child(root_fragment, stacking_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "scene nested stacking oof appends stacking context");
    check(layout_fragment_append_child(root_fragment, sibling_fragment,
                                       make_point(20.0, 20.0))
              == LXB_STATUS_OK,
          "scene nested stacking oof appends sibling");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "scene nested stacking oof stores result root");
    check(layout_result_add_out_of_flow_positioned(result, high_oof, root,
                                                   high_oof_fragment,
                                                   make_point(20.0, 20.0),
                                                   false)
              == LXB_STATUS_OK,
          "scene nested stacking oof registers root geometry");
    freeze_result(result);

    plan = layout_scene_plan_create(layout);
    check(plan != NULL, "scene nested stacking oof plan creates");
    if (plan == NULL) {
        return;
    }

    check(layout_scene_plan_build(plan, result) == LXB_STATUS_OK,
          "scene nested stacking oof plan builds");
    check(layout_scene_plan_node_count(plan) == 4,
          "scene nested stacking oof includes root, context, oof, sibling");

    for (size_t i = 0; i < layout_scene_plan_node_count(plan); i++) {
        const layout_scene_node_t *node = layout_scene_plan_node_at(plan, i);
        layout_fragment_key_t key = layout_scene_node_key(node);

        if (fragment_key_equal(key, layout_fragment_key(stacking_fragment))) {
            stacking_node = node;
        }
        else if (fragment_key_equal(key,
                                    layout_fragment_key(sibling_fragment))) {
            sibling_node = node;
        }
        else if (fragment_key_equal(key,
                                    layout_fragment_key(high_oof_fragment))) {
            oof_node = node;
        }
    }

    check(stacking_node != NULL && sibling_node != NULL && oof_node != NULL,
          "scene nested stacking oof nodes are present");
    check(layout_scene_node_parent_index(oof_node)
              == layout_scene_node_index(stacking_node)
              && layout_scene_node_child_count(stacking_node) == 1
              && layout_scene_node_child_slot(oof_node) == 0,
          "scene nested stacking context owns OOF retained child");
    check(layout_scene_node_parent_index(sibling_node) == 0
              && layout_scene_node_child_slot(sibling_node) == 1,
          "scene nested stacking sibling remains under root after context");
    check(layout_scene_node_stacking_order(oof_node) == 10
              && layout_scene_node_placement(oof_node)
                     == LAYOUT_FRAGMENT_PLACEMENT_OUT_OF_FLOW,
          "scene nested stacking oof keeps placement and z metadata");
    offset = layout_scene_node_offset(oof_node);
    check(offset.x == 10.0 && offset.y == 10.0,
          "scene nested stacking oof offset maps into paint parent space");

    layout_scene_plan_destroy(plan, true);
}

static void
layout_scene_node_stacking_context_smoke(layout_t *layout,
                                         layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *transformed = make_object(layout);
    layout_object_t *opacity = make_object(layout);
    layout_object_t *positioned_z = make_object(layout);
    layout_object_t *fixed = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 240.0,
                                                     120.0);
    layout_fragment_t *transformed_fragment;
    layout_fragment_t *opacity_fragment = make_fragment(result, opacity,
                                                       20.0, 20.0);
    layout_fragment_t *positioned_fragment = make_fragment(result,
                                                          positioned_z,
                                                          20.0, 20.0);
    layout_fragment_t *fixed_fragment = make_fragment(result, fixed, 20.0,
                                                     20.0);
    layout_fragment_init_t transformed_init;
    layout_scene_plan_t *plan = NULL;
    const layout_scene_node_t *transformed_node = NULL;
    const layout_scene_node_t *opacity_node = NULL;
    const layout_scene_node_t *positioned_node = NULL;
    const layout_scene_node_t *fixed_node = NULL;

    memset(&transformed_init, 0, sizeof(transformed_init));
    transformed_init.object = transformed;
    transformed_init.size.width = 20.0;
    transformed_init.size.height = 20.0;
    transformed_init.type = LAYOUT_FRAGMENT_BOX;
    transformed_init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    transformed_init.transform.a = 1.0;
    transformed_init.transform.d = 1.0;
    transformed_init.transform.e = 4.0;
    transformed_fragment = layout_fragment_create(result, &transformed_init);

    check(layout_object_set_parent(transformed, root) == LXB_STATUS_OK,
          "stacking context transformed parent set");
    check(layout_object_set_parent(opacity, root) == LXB_STATUS_OK,
          "stacking context opacity parent set");
    check(layout_object_set_parent(positioned_z, root) == LXB_STATUS_OK,
          "stacking context positioned parent set");
    check(layout_object_set_parent(fixed, root) == LXB_STATUS_OK,
          "stacking context fixed parent set");
    set_opacity(opacity, 0.5);
    set_position(positioned_z, LXB_CSS_POSITION_RELATIVE);
    set_z_index(positioned_z, 7);
    set_position(fixed, LXB_CSS_POSITION_FIXED);
    check(root_fragment != NULL && transformed_fragment != NULL
              && opacity_fragment != NULL && positioned_fragment != NULL
              && fixed_fragment != NULL,
          "stacking context fragments allocate");

    check(layout_fragment_append_child(root_fragment, transformed_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "stacking context appends transformed child");
    check(layout_fragment_append_child(root_fragment, opacity_fragment,
                                       make_point(40.0, 10.0))
              == LXB_STATUS_OK,
          "stacking context appends opacity child");
    check(layout_fragment_append_child(root_fragment, positioned_fragment,
                                       make_point(70.0, 10.0))
              == LXB_STATUS_OK,
          "stacking context appends positioned z child");
    check(layout_fragment_append_child(root_fragment, fixed_fragment,
                                       make_point(100.0, 10.0))
              == LXB_STATUS_OK,
          "stacking context appends fixed child");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "stacking context stores result root");
    freeze_result(result);

    plan = layout_scene_plan_create(layout);
    check(plan != NULL, "stacking context scene plan creates");
    if (plan == NULL) {
        return;
    }

    check(layout_scene_plan_build(plan, result) == LXB_STATUS_OK,
          "stacking context scene plan builds");

    for (size_t i = 0; i < layout_scene_plan_node_count(plan); i++) {
        const layout_scene_node_t *node = layout_scene_plan_node_at(plan, i);
        layout_fragment_key_t key = layout_scene_node_key(node);

        if (fragment_key_equal(key, layout_fragment_key(transformed_fragment))) {
            transformed_node = node;
        }
        else if (fragment_key_equal(key, layout_fragment_key(opacity_fragment))) {
            opacity_node = node;
        }
        else if (fragment_key_equal(key,
                                    layout_fragment_key(positioned_fragment))) {
            positioned_node = node;
        }
        else if (fragment_key_equal(key, layout_fragment_key(fixed_fragment))) {
            fixed_node = node;
        }
    }

    check(transformed_node != NULL && opacity_node != NULL
              && positioned_node != NULL && fixed_node != NULL,
          "stacking context nodes are present");
    check((layout_scene_node_flags(transformed_node)
           & LAYOUT_SCENE_NODE_STACKING_CONTEXT) != 0
              && (layout_scene_node_flags(opacity_node)
                  & LAYOUT_SCENE_NODE_STACKING_CONTEXT) != 0
              && (layout_scene_node_flags(positioned_node)
                  & LAYOUT_SCENE_NODE_STACKING_CONTEXT) != 0
              && (layout_scene_node_flags(fixed_node)
                  & LAYOUT_SCENE_NODE_STACKING_CONTEXT) != 0,
          "scene nodes flag stacking context creators");
    check((layout_scene_node_layer_hints(transformed_node)
           & LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY) != 0
              && (layout_scene_node_layer_hints(opacity_node)
                  & LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY) != 0
              && (layout_scene_node_layer_hints(positioned_node)
                  & LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY) != 0
              && (layout_scene_node_layer_hints(fixed_node)
                  & LAYOUT_SCENE_LAYER_HINT_REPAINT_BOUNDARY) != 0,
          "stacking contexts expose repaint boundary hints");

    layout_scene_plan_destroy(plan, true);
}

static void
layout_scene_plan_diff_smoke(layout_t *layout)
{
    layout_result_t *old_result = layout_result_create(layout);
    layout_result_t *new_result = layout_result_create(layout);
    layout_object_t *root = make_object(layout);
    layout_object_t *kept = make_object(layout);
    layout_object_t *moved = make_object(layout);
    layout_object_t *updated = make_object(layout);
    layout_object_t *removed = make_object(layout);
    layout_object_t *inserted = make_object(layout);
    layout_fragment_t *old_root;
    layout_fragment_t *old_kept;
    layout_fragment_t *old_moved;
    layout_fragment_t *old_updated;
    layout_fragment_t *old_removed;
    layout_fragment_t *new_root;
    layout_fragment_t *new_kept;
    layout_fragment_t *new_moved;
    layout_fragment_t *new_updated;
    layout_fragment_t *new_inserted;
    layout_scene_plan_t *old_plan = NULL;
    layout_scene_plan_t *new_plan = NULL;
    layout_scene_diff_t *diff = NULL;
    size_t keep_count = 0;
    size_t move_count = 0;
    size_t insert_count = 0;
    size_t remove_count = 0;
    size_t update_count = 0;
    size_t geometry_patch_index = LAYOUT_SCENE_NODE_NONE;
    size_t moved_patch_index = LAYOUT_SCENE_NODE_NONE;
    size_t kept_move_patch_index = LAYOUT_SCENE_NODE_NONE;

    check(old_result != NULL && new_result != NULL,
          "scene diff results allocate");
    if (old_result == NULL || new_result == NULL) {
        goto done;
    }

    old_root = make_fragment(old_result, root, 200.0, 140.0);
    old_kept = make_fragment(old_result, kept, 20.0, 20.0);
    old_moved = make_fragment(old_result, moved, 20.0, 20.0);
    old_updated = make_fragment(old_result, updated, 20.0, 20.0);
    old_removed = make_fragment(old_result, removed, 20.0, 20.0);
    check(old_root != NULL && old_kept != NULL && old_moved != NULL
              && old_updated != NULL && old_removed != NULL,
          "scene diff old fragments allocate");
    check(layout_fragment_append_child(old_root, old_kept,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff old appends kept");
    check(layout_fragment_append_child(old_root, old_moved,
                                       make_point(30.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff old appends moved");
    check(layout_fragment_append_child(old_root, old_updated,
                                       make_point(60.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff old appends updated");
    check(layout_fragment_append_child(old_root, old_removed,
                                       make_point(90.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff old appends removed");
    check(layout_result_set_root_fragment(old_result, old_root)
              == LXB_STATUS_OK,
          "scene diff old result stores root");
    freeze_result(old_result);

    new_root = make_fragment(new_result, root, 200.0, 140.0);
    new_moved = make_fragment(new_result, moved, 20.0, 20.0);
    new_kept = make_fragment(new_result, kept, 20.0, 20.0);
    new_updated = make_fragment(new_result, updated, 30.0, 20.0);
    new_inserted = make_fragment(new_result, inserted, 20.0, 20.0);
    check(new_root != NULL && new_kept != NULL && new_moved != NULL
              && new_updated != NULL && new_inserted != NULL,
          "scene diff new fragments allocate");
    check(layout_fragment_append_child(new_root, new_moved,
                                       make_point(30.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff new appends moved first");
    check(layout_fragment_append_child(new_root, new_kept,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff new appends kept second");
    check(layout_fragment_append_child(new_root, new_updated,
                                       make_point(60.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff new appends updated");
    check(layout_fragment_append_child(new_root, new_inserted,
                                       make_point(90.0, 0.0))
              == LXB_STATUS_OK,
          "scene diff new appends inserted");
    check(layout_result_set_root_fragment(new_result, new_root)
              == LXB_STATUS_OK,
          "scene diff new result stores root");
    freeze_result(new_result);

    old_plan = layout_scene_plan_create(layout);
    new_plan = layout_scene_plan_create(layout);
    diff = layout_scene_diff_create();
    check(old_plan != NULL && new_plan != NULL && diff != NULL,
          "scene diff plans allocate");
    if (old_plan == NULL || new_plan == NULL || diff == NULL) {
        goto done;
    }

    check(layout_scene_plan_build(old_plan, old_result) == LXB_STATUS_OK,
          "scene diff old plan builds");
    check(layout_scene_plan_build(new_plan, new_result) == LXB_STATUS_OK,
          "scene diff new plan builds");
    check(layout_scene_plan_diff(old_plan, new_plan, diff) == LXB_STATUS_OK,
          "scene plan diff runs");
    check(layout_scene_diff_old_generation(diff)
                  == layout_scene_plan_generation(old_plan)
              && layout_scene_diff_new_generation(diff)
                     == layout_scene_plan_generation(new_plan),
          "scene plan diff carries generation tokens");

    for (size_t i = 0; i < layout_scene_diff_patch_count(diff); i++) {
        const layout_scene_patch_t *patch = layout_scene_diff_patch_at(diff,
                                                                       i);

        switch (layout_scene_patch_op(patch)) {
        case LAYOUT_SCENE_PATCH_KEEP:
            keep_count++;
            break;

        case LAYOUT_SCENE_PATCH_MOVE:
            move_count++;
            if (fragment_key_equal(layout_scene_patch_key(patch),
                                   layout_fragment_key(new_moved))) {
                moved_patch_index = i;
            }
            if (fragment_key_equal(layout_scene_patch_key(patch),
                                   layout_fragment_key(new_kept))) {
                kept_move_patch_index = i;
            }
            if ((layout_scene_patch_dirty_bits(patch)
                 & LAYOUT_SCENE_DIRTY_GEOMETRY) != 0) {
                geometry_patch_index = i;
            }
            break;

        case LAYOUT_SCENE_PATCH_INSERT:
            insert_count++;
            break;

        case LAYOUT_SCENE_PATCH_REMOVE:
            remove_count++;
            break;

        case LAYOUT_SCENE_PATCH_UPDATE:
            update_count++;
            if ((layout_scene_patch_dirty_bits(patch)
                 & LAYOUT_SCENE_DIRTY_GEOMETRY) != 0) {
                geometry_patch_index = i;
            }
            break;
        }
    }

    check(layout_scene_diff_patch_count(diff) == 6,
          "scene diff emits remove plus one patch per new node");
    check(keep_count == 1, "scene diff keeps root");
    check(move_count == 3, "scene diff detects retained child slot moves");
    check(insert_count == 1, "scene diff detects inserted child");
    check(remove_count == 1, "scene diff detects removed child");
    check(update_count == 0,
          "scene diff folds geometry into move when slot also changes");
    if (geometry_patch_index != LAYOUT_SCENE_NODE_NONE) {
        const layout_scene_patch_t *patch =
            layout_scene_diff_patch_at(diff, geometry_patch_index);

        check((layout_scene_patch_dirty_bits(patch)
               & LAYOUT_SCENE_DIRTY_GEOMETRY) != 0,
              "scene diff marks geometry dirty for size change");
        check(fragment_key_equal(layout_scene_patch_key(patch),
                                 layout_fragment_key(new_updated)),
              "scene diff geometry dirty stays on updated child");
    }
    if (moved_patch_index != LAYOUT_SCENE_NODE_NONE) {
        const layout_scene_patch_t *patch =
            layout_scene_diff_patch_at(diff, moved_patch_index);

        check(layout_scene_patch_old_slot(patch) == 1
                  && layout_scene_patch_new_slot(patch) == 0
                  && layout_scene_patch_new_previous_sibling_index(patch)
                         == LAYOUT_SCENE_NODE_NONE
                  && fragment_key_empty(
                         layout_scene_patch_new_previous_sibling_key(patch)),
              "scene diff move carries indexed slot for first child");
    }
    if (kept_move_patch_index != LAYOUT_SCENE_NODE_NONE) {
        const layout_scene_patch_t *patch =
            layout_scene_diff_patch_at(diff, kept_move_patch_index);

        check(layout_scene_patch_old_slot(patch) == 0
                  && layout_scene_patch_new_slot(patch) == 1
                  && fragment_key_equal(
                         layout_scene_patch_new_previous_sibling_key(patch),
                         layout_fragment_key(new_moved)),
              "scene diff move carries previous sibling key");
    }

done:
    layout_scene_diff_destroy(diff, true);
    layout_scene_plan_destroy(new_plan, true);
    layout_scene_plan_destroy(old_plan, true);
    layout_result_destroy(new_result, true);
    layout_result_destroy(old_result, true);
}

static void
overflow_clip_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *no_clip = make_object(layout);
    layout_object_t *x_clip = make_object(layout);
    layout_object_t *y_clip = make_object(layout);
    layout_object_t *both_clip = make_object(layout);
    layout_object_t *clip_path = make_object(layout);
    layout_object_t *no_clip_child = make_object(layout);
    layout_object_t *x_outside = make_object(layout);
    layout_object_t *y_outside = make_object(layout);
    layout_object_t *both_outside = make_object(layout);
    layout_object_t *clip_path_outside = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 300.0,
                                                     360.0);
    layout_fragment_t *no_clip_fragment = make_fragment(result, no_clip,
                                                        100.0, 100.0);
    layout_fragment_t *x_clip_fragment = make_fragment(result, x_clip, 100.0,
                                                       100.0);
    layout_fragment_t *y_clip_fragment = make_fragment(result, y_clip, 100.0,
                                                       100.0);
    layout_fragment_t *both_clip_fragment = make_fragment(result, both_clip,
                                                          100.0, 100.0);
    layout_fragment_t *clip_path_fragment = make_fragment(result, clip_path,
                                                          100.0, 100.0);
    layout_fragment_t *no_clip_child_fragment = make_fragment(result,
                                                              no_clip_child,
                                                              30.0, 30.0);
    layout_fragment_t *x_outside_fragment = make_fragment(result, x_outside,
                                                          30.0, 30.0);
    layout_fragment_t *y_outside_fragment = make_fragment(result, y_outside,
                                                          30.0, 30.0);
    layout_fragment_t *both_outside_fragment = make_fragment(result,
                                                             both_outside,
                                                             30.0, 30.0);
    layout_fragment_t *clip_path_outside_fragment =
        make_fragment(result, clip_path_outside, 30.0, 30.0);
    layout_fragment_t *hit = NULL;

    set_overflow(x_clip, LXB_CSS_OVERFLOW_X_HIDDEN,
                 LXB_CSS_OVERFLOW_Y_VISIBLE);
    set_overflow(y_clip, LXB_CSS_OVERFLOW_X_VISIBLE,
                 LXB_CSS_OVERFLOW_Y_HIDDEN);
    set_overflow(both_clip, LXB_CSS_OVERFLOW_X_CLIP,
                 LXB_CSS_OVERFLOW_Y_HIDDEN);
    layout_object_set_has_clip_path(clip_path, true);

    check(layout_object_set_parent(no_clip, root) == LXB_STATUS_OK,
          "set no-clip parent");
    check(layout_object_set_parent(x_clip, root) == LXB_STATUS_OK,
          "set x clip parent");
    check(layout_object_set_parent(y_clip, root) == LXB_STATUS_OK,
          "set y clip parent");
    check(layout_object_set_parent(both_clip, root) == LXB_STATUS_OK,
          "set both clip parent");
    check(layout_object_set_parent(clip_path, root) == LXB_STATUS_OK,
          "set clip-path parent");
    check(layout_object_set_parent(no_clip_child, no_clip) == LXB_STATUS_OK,
          "set no-clip child parent");
    check(layout_object_set_parent(x_outside, x_clip) == LXB_STATUS_OK,
          "set x clipped child parent");
    check(layout_object_set_parent(y_outside, y_clip) == LXB_STATUS_OK,
          "set y clipped child parent");
    check(layout_object_set_parent(both_outside, both_clip) == LXB_STATUS_OK,
          "set both clipped child parent");
    check(layout_object_set_parent(clip_path_outside, clip_path)
              == LXB_STATUS_OK,
          "set clip-path clipped child parent");
    check(layout_fragment_append_child(root_fragment, no_clip_fragment,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_OK,
          "append no-clip fragment");
    check(layout_fragment_append_child(root_fragment, x_clip_fragment,
                                       make_point(0.0, 110.0))
              == LXB_STATUS_OK,
          "append x clip fragment");
    check(layout_fragment_append_child(root_fragment, y_clip_fragment,
                                       make_point(110.0, 0.0))
              == LXB_STATUS_OK,
          "append y clip fragment");
    check(layout_fragment_append_child(root_fragment, both_clip_fragment,
                                       make_point(110.0, 110.0))
              == LXB_STATUS_OK,
          "append both clip fragment");
    check(layout_fragment_append_child(root_fragment, clip_path_fragment,
                                       make_point(0.0, 220.0))
              == LXB_STATUS_OK,
          "append clip-path fragment");
    check(layout_fragment_append_child(no_clip_fragment,
                                       no_clip_child_fragment,
                                       make_point(220.0, 10.0))
              == LXB_STATUS_OK,
          "append no-clip overflowing child fragment");
    check(layout_fragment_append_child(x_clip_fragment, x_outside_fragment,
                                       make_point(120.0, 10.0))
              == LXB_STATUS_OK,
          "append x clipped child fragment");
    check(layout_fragment_append_child(y_clip_fragment, y_outside_fragment,
                                       make_point(10.0, 120.0))
              == LXB_STATUS_OK,
          "append y clipped child fragment");
    check(layout_fragment_append_child(both_clip_fragment,
                                       both_outside_fragment,
                                       make_point(120.0, 120.0))
              == LXB_STATUS_OK,
          "append both clipped child fragment");
    check(layout_fragment_append_child(clip_path_fragment,
                                       clip_path_outside_fragment,
                                       make_point(120.0, 10.0))
              == LXB_STATUS_OK,
          "append clip-path clipped child fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "overflow clip smoke stores result root");
    freeze_result(result);

    check(layout_object_overflow_clip(no_clip) == LAYOUT_OVERFLOW_CLIP_NONE,
          "visible overflow maps to no clip");
    check(layout_object_overflow_clip(x_clip) == LAYOUT_OVERFLOW_CLIP_X,
          "computed overflow hidden on x maps to x clip");
    check(layout_object_overflow_clip(y_clip) == LAYOUT_OVERFLOW_CLIP_Y,
          "computed overflow hidden on y maps to y clip");
    check(layout_object_overflow_clip(both_clip) == LAYOUT_OVERFLOW_CLIP_BOTH,
          "computed overflow clip/hidden maps to both-axis clip");
    check(layout_object_has_clip_path(clip_path),
          "clip-path reservation bit is observable");

    check(layout_hit_test(result, root_fragment, make_point(230.0, 20.0),
                          &hit)
              == LXB_STATUS_OK,
          "no-clip overflowing child can be hit");
    check(hit == no_clip_child_fragment,
          "no clip preserves hit testing outside parent border box");
    check(layout_hit_test(result, root_fragment, make_point(130.0, 20.0),
                          &hit)
              == LXB_STATUS_OK,
          "later visual sibling can cover no-clip overflow");
    check(hit == y_clip_fragment,
          "hit test uses visual order instead of DOM parent order");
    check(layout_hit_test(result, root_fragment, make_point(130.0, 120.0),
                          &hit)
              == LXB_STATUS_OK,
          "x-clipped point may still hit root");
    check(hit != x_outside_fragment,
          "x clipped invisible area cannot hit child");
    check(layout_hit_test(result, root_fragment, make_point(120.0, 130.0),
                          &hit)
              == LXB_STATUS_OK,
          "y-clipped point may still hit root");
    check(hit != y_outside_fragment,
          "y clipped invisible area cannot hit child");
    check(layout_hit_test(result, root_fragment, make_point(230.0, 230.0),
                          &hit)
              == LXB_STATUS_OK,
          "both-clipped point may still hit root");
    check(hit != both_outside_fragment,
          "both-axis clipped invisible area cannot hit child");
    check(layout_hit_test(result, root_fragment, make_point(130.0, 230.0),
                          &hit)
              == LXB_STATUS_OK,
          "clip-path reserved point may still hit root");
    check(hit != clip_path_outside_fragment,
          "clip-path reserved rectangular clip blocks outside hit");
}

static void
transform_smoke(layout_t *layout, layout_result_t *result)
{
    layout_object_t *root = make_object(layout);
    layout_object_t *scaled = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 300.0,
                                                     300.0);
    layout_fragment_init_t init;
    layout_fragment_t *scaled_fragment;
    layout_point_t local = {0.0, 0.0};
    layout_rect_t local_rect;
    layout_rect_t root_rect;
    layout_fragment_t *hit = NULL;
    layout_fragment_t *mapped_fragment = NULL;

    memset(&init, 0, sizeof(init));
    init.object = scaled;
    init.size.width = 20.0;
    init.size.height = 20.0;
    init.type = LAYOUT_FRAGMENT_BOX;
    init.box_type = LAYOUT_FRAGMENT_BOX_NORMAL;
    init.transform.a = 2.0;
    init.transform.d = 2.0;
    local_rect.x = 0.0;
    local_rect.y = 0.0;
    local_rect.width = 20.0;
    local_rect.height = 20.0;

    scaled_fragment = layout_fragment_create(result, &init);

    check(layout_object_set_parent(scaled, root) == LXB_STATUS_OK,
          "set scaled object parent");
    check(root_fragment != NULL && scaled_fragment != NULL,
          "transform fragments allocate");
    check(layout_fragment_append_child(root_fragment, scaled_fragment,
                                       make_point(10.0, 10.0))
              == LXB_STATUS_OK,
          "append scaled fragment");
    check(layout_result_set_root_fragment(result, root_fragment)
              == LXB_STATUS_OK,
          "transform smoke stores result root");
    freeze_result(result);
    check(layout_result_point_in_fragment(result, scaled_fragment,
                                          make_point(30.0, 30.0),
                                          &local)
              == LXB_STATUS_OK,
          "root point maps to transformed local point");
    check(local.x > 9.99 && local.x < 10.01 && local.y > 9.99
              && local.y < 10.01,
          "transform inverse maps point through 2d affine matrix");
    check(layout_result_root_point_to_object(result, scaled, 0,
                                             make_point(30.0, 30.0),
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &local, &mapped_fragment)
              == LXB_STATUS_OK,
          "root point maps to layout object local coordinates");
    check(mapped_fragment == scaled_fragment,
          "root point to object reports mapped physical fragment");
    check(local.x > 9.99 && local.x < 10.01 && local.y > 9.99
              && local.y < 10.01,
          "object coordinate mapping applies transform state");
    check(layout_result_root_point_to_object(result, scaled, 0,
                                             make_point(30.0, 30.0),
                                             LAYOUT_COORDINATE_IGNORE_TRANSFORMS,
                                             &local, NULL)
              == LXB_STATUS_OK,
          "object coordinate mapping can ignore transforms");
    check(local.x == 20.0 && local.y == 20.0,
          "ignore-transform mode only subtracts fragment offsets");
    check(layout_result_fragment_rect_to_root(result, scaled_fragment,
                                             local_rect,
                                             LAYOUT_COORDINATE_APPLY_TRANSFORMS,
                                             &root_rect)
              == LXB_STATUS_OK,
          "local rect maps to root enclosing rect");
    check(root_rect.x == 10.0 && root_rect.y == 10.0
              && root_rect.width == 40.0 && root_rect.height == 40.0,
          "local rect mapping applies offset and affine transform");
    check(layout_hit_test(result, root_fragment, make_point(30.0, 30.0),
                          &hit)
              == LXB_STATUS_OK,
          "transformed fragment hit");
    check(hit == scaled_fragment, "hit test respects transformed geometry");
}

static void
layout_result_boundary_smoke(layout_t *layout, layout_result_t *result)
{
    layout_result_t *other_result = layout_result_create(layout);
    layout_object_t *root = make_object(layout);
    layout_object_t *child = make_object(layout);
    layout_fragment_t *root_fragment = make_fragment(result, root, 100.0,
                                                     100.0);
    layout_fragment_t *foreign_fragment;
    layout_fragment_t *hit = NULL;

    check(other_result != NULL, "second layout result allocates");
    foreign_fragment = make_fragment(other_result, child, 10.0, 10.0);

    check(root_fragment != NULL && foreign_fragment != NULL,
          "boundary smoke fragments allocate");
    check(layout_fragment_append_child(root_fragment, foreign_fragment,
                                       make_point(0.0, 0.0))
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "fragment links cannot cross layout results");
    check(layout_hit_test(other_result, root_fragment, make_point(1.0, 1.0),
                          &hit)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "hit test rejects root fragment from a different layout result");
    check(hit == NULL, "cross-result hit test returns no fragment");
    check(layout_result_set_root_fragment(other_result, root_fragment)
              == LXB_STATUS_ERROR_WRONG_ARGS,
          "layout result rejects foreign root fragment");

    layout_result_destroy(other_result, true);
}

static void
lifecycle_smoke(void)
{
    for (int i = 0; i < 32; i++) {
        layout_t *layout = layout_create();
        layout_object_t *object;
        layout_result_t *result;
        layout_fragment_t *fragment;

        check(layout != NULL, "layout create returns object");
        check(layout_init(layout) == LXB_STATUS_OK, "layout init succeeds");
        object = make_object(layout);
        check(object != NULL, "object allocates from Lexbor dobject arena");
        result = layout_result_create(layout);
        check(result != NULL, "layout result allocates");
        fragment = make_fragment(result, object, 10.0, 10.0);
        check(fragment != NULL, "fragment allocates from result arena");
        layout_result_destroy(result, true);
        layout = layout_destroy(layout, true);
        check(layout == NULL, "layout destroy returns null when self destroyed");
    }
}

static void
run_layout_smoke(void (*smoke)(layout_t *layout))
{
    layout_t *layout = layout_create();
    lxb_status_t status;

    check(layout != NULL, "layout create returns object");
    if (layout == NULL) {
        return;
    }

    status = layout_init(layout);
    check(status == LXB_STATUS_OK, "layout init succeeds");
    if (status != LXB_STATUS_OK) {
        layout_destroy(layout, true);
        return;
    }

    smoke(layout);
    layout_destroy(layout, true);
}

static void
run_layout_result_smoke(void (*smoke)(layout_t *layout,
                                      layout_result_t *result))
{
    layout_t *layout = layout_create();
    layout_result_t *result;
    lxb_status_t status;

    check(layout != NULL, "layout create returns object");
    if (layout == NULL) {
        return;
    }

    status = layout_init(layout);
    check(status == LXB_STATUS_OK, "layout init succeeds");
    if (status != LXB_STATUS_OK) {
        layout_destroy(layout, true);
        return;
    }

    result = layout_result_create(layout);
    check(result != NULL, "layout result create returns object");
    check(layout_result_layout(result) == layout,
          "layout result references owning layout context");
    if (result != NULL) {
        smoke(layout, result);
        layout_result_destroy(result, true);
    }

    layout_destroy(layout, true);
}

int
main(void)
{
    run_layout_smoke(node_style_reference_smoke);
    run_layout_smoke(layout_object_style_derived_bits_smoke);
    run_layout_smoke(layout_object_identity_smoke);
    run_layout_smoke(layout_tree_builder_smoke);
    run_layout_smoke(layout_tree_fragment_builder_smoke);
    run_layout_smoke(layout_block_reflow_auto_height_smoke);
    run_layout_smoke(layout_reflow_input_constraints_smoke);
    run_layout_smoke(positioned_inset_reflow_smoke);
    run_layout_smoke(relative_position_reflow_smoke);
    run_layout_smoke(block_margin_collapse_reflow_smoke);
    run_layout_smoke(empty_block_margin_collapse_reflow_smoke);
    run_layout_smoke(parent_child_margin_collapse_reflow_smoke);
    run_layout_smoke(block_formatting_root_margin_boundary_smoke);
    run_layout_smoke(auto_margin_reflow_smoke);
    run_layout_smoke(float_clear_reflow_smoke);
    run_layout_smoke(clearance_breaks_margin_collapse_reflow_smoke);
    run_layout_smoke(float_escape_from_non_bfc_reflow_smoke);
    run_layout_smoke(float_escape_uses_non_bfc_content_offset_reflow_smoke);
    run_layout_smoke(flow_root_contains_float_reflow_smoke);
    run_layout_smoke(float_available_space_reflow_smoke);
    run_layout_smoke(layout_object_tree_smoke);
    run_layout_smoke(layout_object_child_list_oof_dirty_smoke);
    run_layout_smoke(layout_object_child_list_clear_smoke);
    run_layout_smoke(layout_object_state_smoke);
    run_layout_smoke(containing_block_smoke);
    run_layout_smoke(containing_block_cached_bit_smoke);
    run_layout_result_smoke(dirty_guard_smoke);
    run_layout_result_smoke(dirty_bits_guard_smoke);
    run_layout_result_smoke(fragment_subtree_dirty_guard_smoke);
    run_layout_result_smoke(out_of_flow_geometry_owner_smoke);
    run_layout_result_smoke(out_of_flow_dirty_guard_smoke);
    run_layout_result_smoke(out_of_flow_containing_fragment_smoke);
    run_layout_result_smoke(hit_order_smoke);
    run_layout_result_smoke(hit_order_paint_phase_smoke);
    run_layout_result_smoke(hit_order_out_of_flow_smoke);
    run_layout_result_smoke(hit_nested_stacking_context_oof_smoke);
    run_layout_result_smoke(hit_nested_out_of_flow_smoke);
    run_layout_result_smoke(hit_may_intersect_smoke);
    run_layout_result_smoke(physical_fragment_link_offset_smoke);
    run_layout_result_smoke(physical_fragment_retained_identity_smoke);
    run_layout_result_smoke(physical_fragment_subtree_reuse_smoke);
    run_layout_result_smoke(physical_fragment_result_freeze_smoke);
    run_layout_result_smoke(physical_fragment_traversal_guard_smoke);
    run_layout_result_smoke(fragment_paint_flags_hit_test_smoke);
    run_layout_result_smoke(fragment_data_smoke);
    run_layout_result_smoke(native_embed_host_token_smoke);
    run_layout_result_smoke(layout_scene_plan_build_smoke);
    run_layout_result_smoke(layout_scene_plan_paint_order_oof_smoke);
    run_layout_result_smoke(layout_scene_plan_nested_stacking_context_oof_smoke);
    run_layout_result_smoke(layout_scene_node_stacking_context_smoke);
    run_layout_smoke(layout_scene_plan_diff_smoke);
    run_layout_result_smoke(overflow_clip_smoke);
    run_layout_result_smoke(transform_smoke);
    run_layout_result_smoke(layout_result_boundary_smoke);
    lifecycle_smoke();

    if (failed) {
        printf("\nlayout_smoke failed\n");
        return 1;
    }

    printf("\nlayout_smoke passed\n");
    return 0;
}
