/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/style/resolver.h"
#include "lexbor/style/property_compute.h"
#include "lexbor/style/dom/interfaces/element.h"

struct lxb_style_resolver {
    lxb_style_resolve_context_t  ctx;
    lxb_style_computed_t         initial;
    uintptr_t                    cache_id;
    size_t                       generation;
    bool                         initialized;
};


static uintptr_t lxb_style_resolver_cache_id_next = 1;


static lxb_status_t
lxb_style_resolver_resolve_subtree(lxb_style_resolver_t *resolver,
                                   lxb_dom_node_t *node);

static lxb_status_t
lxb_style_resolver_initial_set(lxb_style_resolver_t *resolver);

static const lxb_style_resolve_context_t
lxb_style_resolver_default_context = {
    1024.0,
    768.0,
    16.0,
    19.2
};

static const lxb_css_property_type_t lxb_style_compute_order[] = {
    LXB_CSS_PROPERTY_DIRECTION,
    LXB_CSS_PROPERTY_WRITING_MODE,
    LXB_CSS_PROPERTY_COLOR,
    LXB_CSS_PROPERTY_FONT_SIZE,
    LXB_CSS_PROPERTY_FONT_WEIGHT,
    LXB_CSS_PROPERTY_FONT_STYLE,
    LXB_CSS_PROPERTY_FONT_STRETCH,
    LXB_CSS_PROPERTY_LINE_HEIGHT,
    LXB_CSS_PROPERTY_DISPLAY,
    LXB_CSS_PROPERTY_VISIBILITY,
    LXB_CSS_PROPERTY_POSITION,
    LXB_CSS_PROPERTY_BOX_SIZING,
    LXB_CSS_PROPERTY_FLOAT,
    LXB_CSS_PROPERTY_CLEAR,
    LXB_CSS_PROPERTY_WIDTH,
    LXB_CSS_PROPERTY_HEIGHT,
    LXB_CSS_PROPERTY_MIN_WIDTH,
    LXB_CSS_PROPERTY_MIN_HEIGHT,
    LXB_CSS_PROPERTY_MAX_WIDTH,
    LXB_CSS_PROPERTY_MAX_HEIGHT,
    LXB_CSS_PROPERTY_TOP,
    LXB_CSS_PROPERTY_RIGHT,
    LXB_CSS_PROPERTY_BOTTOM,
    LXB_CSS_PROPERTY_LEFT,
    LXB_CSS_PROPERTY_MARGIN,
    LXB_CSS_PROPERTY_MARGIN_TOP,
    LXB_CSS_PROPERTY_MARGIN_RIGHT,
    LXB_CSS_PROPERTY_MARGIN_BOTTOM,
    LXB_CSS_PROPERTY_MARGIN_LEFT,
    LXB_CSS_PROPERTY_PADDING,
    LXB_CSS_PROPERTY_PADDING_TOP,
    LXB_CSS_PROPERTY_PADDING_RIGHT,
    LXB_CSS_PROPERTY_PADDING_BOTTOM,
    LXB_CSS_PROPERTY_PADDING_LEFT,
    LXB_CSS_PROPERTY_BORDER,
    LXB_CSS_PROPERTY_BORDER_TOP,
    LXB_CSS_PROPERTY_BORDER_RIGHT,
    LXB_CSS_PROPERTY_BORDER_BOTTOM,
    LXB_CSS_PROPERTY_BORDER_LEFT,
    LXB_CSS_PROPERTY_BORDER_TOP_COLOR,
    LXB_CSS_PROPERTY_BORDER_RIGHT_COLOR,
    LXB_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
    LXB_CSS_PROPERTY_BORDER_LEFT_COLOR,
    LXB_CSS_PROPERTY_BACKGROUND_COLOR,
    LXB_CSS_PROPERTY_OVERFLOW_X,
    LXB_CSS_PROPERTY_OVERFLOW_Y,
    LXB_CSS_PROPERTY_OPACITY,
    LXB_CSS_PROPERTY_Z_INDEX,
    LXB_CSS_PROPERTY_TEXT_ALIGN,
    LXB_CSS_PROPERTY_WHITE_SPACE
};


lxb_style_resolver_t *
lxb_style_resolver_create(void)
{
    return lexbor_calloc(1, sizeof(lxb_style_resolver_t));
}

lxb_status_t
lxb_style_resolver_init(lxb_style_resolver_t *resolver)
{
    lxb_status_t status;

    if (resolver == NULL) {
        return LXB_STATUS_ERROR_OBJECT_IS_NULL;
    }

    resolver->ctx = lxb_style_resolver_default_context;
    resolver->cache_id = lxb_style_resolver_cache_id_next++;
    if (resolver->cache_id == 0) {
        resolver->cache_id = lxb_style_resolver_cache_id_next++;
    }

    resolver->generation = 1;

    status = lxb_style_resolver_initial_set(resolver);
    if (status != LXB_STATUS_OK) {
        return status;
    }

    resolver->initialized = true;

    return LXB_STATUS_OK;
}

void
lxb_style_resolver_clean(lxb_style_resolver_t *resolver)
{
    if (resolver == NULL || !resolver->initialized) {
        return;
    }

    resolver->generation++;
    if (resolver->generation == 0) {
        resolver->generation++;
    }

    (void) lxb_style_resolver_initial_set(resolver);
}

lxb_style_resolver_t *
lxb_style_resolver_destroy(lxb_style_resolver_t *resolver, bool self_destroy)
{
    if (resolver == NULL) {
        return NULL;
    }

    if (resolver->initialized) {
        lxb_style_resolver_clean(resolver);
        lxb_style_computed_destroy(&resolver->initial);
    }

    if (self_destroy) {
        return lexbor_free(resolver);
    }

    return resolver;
}

void
lxb_style_resolver_context_set(lxb_style_resolver_t *resolver,
                               const lxb_style_resolve_context_t *ctx)
{
    if (resolver == NULL || ctx == NULL) {
        return;
    }

    resolver->ctx = *ctx;

    if (resolver->ctx.initial_font_size <= 0.0) {
        resolver->ctx.initial_font_size = 16.0;
    }

    if (resolver->ctx.initial_line_height <= 0.0) {
        resolver->ctx.initial_line_height = resolver->ctx.initial_font_size * 1.2;
    }

    (void) lxb_style_resolver_initial_set(resolver);
    resolver->generation++;
    if (resolver->generation == 0) {
        resolver->generation++;
    }
}

lxb_status_t
lxb_style_resolver_resolve_document(lxb_style_resolver_t *resolver,
                                    lxb_html_document_t *document)
{
    lxb_dom_node_t *root;

    if (resolver == NULL || document == NULL) {
        return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    root = lxb_dom_document_root(lxb_dom_interface_document(document));
    if (root == NULL) {
        root = lxb_dom_interface_node(document);
    }

    return lxb_style_resolver_resolve_subtree(resolver, root);
}

const lxb_style_computed_t *
lxb_style_resolver_resolve_element(lxb_style_resolver_t *resolver,
                                   lxb_dom_element_t *element)
{
    lxb_dom_node_t *parent_node;
    lxb_style_computed_t *computed, *old;
    const lxb_style_computed_t *parent;
    lxb_style_property_compute_f compute;
    lxb_style_compute_ctx_t ctx;
    lxb_css_property_type_t id;
    size_t i;

    if (resolver == NULL || element == NULL) {
        return NULL;
    }

    old = element->computed_style;

    if (!(element->condition & LXB_DOM_ELEMENT_CONDITION_DIRTY_COMPUTED_STYLE)
        && old != NULL
        && old->cache_id == resolver->cache_id
        && old->cache_generation == resolver->generation)
    {
        return old;
    }

    parent = &resolver->initial;
    parent_node = lxb_dom_interface_node(element)->parent;

    while (parent_node != NULL) {
        if (parent_node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            parent = lxb_style_resolver_resolve_element(resolver,
                                lxb_dom_interface_element(parent_node));
            if (parent == NULL) {
                return NULL;
            }

            break;
        }

        parent_node = parent_node->parent;
    }

    computed = lxb_style_computed_create_initial(&resolver->initial, parent);
    if (computed == NULL) {
        return NULL;
    }

    ctx.resolver = resolver;
    ctx.element = element;
    ctx.parent = parent;
    ctx.style = computed;
    ctx.ctx = &resolver->ctx;

    for (i = 0; i < (sizeof(lxb_style_compute_order)
                    / sizeof(lxb_style_compute_order[0])); i++)
    {
        id = lxb_style_compute_order[i];
        compute = lxb_style_property_compute_by_id(id);
        if (compute != NULL) {
            if (lxb_dom_element_style_by_id(element, id) == NULL) {
                continue;
            }

            if (lxb_style_computed_detach_for_property(computed, id)
                != LXB_STATUS_OK)
            {
                lxb_style_computed_unref(computed);
                return NULL;
            }

            compute(&ctx);
        }
    }

    if (old != NULL && lxb_style_computed_equal(old, computed)) {
        old->cache_id = resolver->cache_id;
        old->cache_generation = resolver->generation;
        lxb_style_computed_unref(computed);
        element->condition &= ~LXB_DOM_ELEMENT_CONDITION_DIRTY_COMPUTED_STYLE;
        return old;
    }

    if (old != NULL) {
        lxb_style_computed_unref(old);
    }

    computed->cache_id = resolver->cache_id;
    computed->cache_generation = resolver->generation;

    element->computed_style = computed;
    element->condition &= ~LXB_DOM_ELEMENT_CONDITION_DIRTY_COMPUTED_STYLE;

    return element->computed_style;
}

const lxb_style_computed_t *
lxb_style_resolver_style_by_element(lxb_style_resolver_t *resolver,
                                    const lxb_dom_element_t *element)
{
    const lxb_style_computed_t *style;

    if (resolver == NULL || element == NULL) {
        return NULL;
    }

    if (element->condition & LXB_DOM_ELEMENT_CONDITION_DIRTY_COMPUTED_STYLE) {
        return NULL;
    }

    style = element->computed_style;

    if (style == NULL || style->cache_id != resolver->cache_id
        || style->cache_generation != resolver->generation)
    {
        return NULL;
    }

    return style;
}

void
lxb_style_resolver_invalidate(lxb_style_resolver_t *resolver,
                              lxb_dom_element_t *element)
{
    if (resolver == NULL) {
        return;
    }

    if (element == NULL) {
        resolver->generation++;
        if (resolver->generation == 0) {
            resolver->generation++;
        }

        return;
    }

    element->condition |= LXB_DOM_ELEMENT_CONDITION_DIRTY_COMPUTED_STYLE;
}

void
lxb_style_resolver_invalidate_subtree(lxb_style_resolver_t *resolver,
                                      lxb_dom_element_t *element)
{
    lxb_dom_node_t *node, *child;

    if (resolver == NULL) {
        return;
    }

    if (element == NULL) {
        resolver->generation++;
        if (resolver->generation == 0) {
            resolver->generation++;
        }

        return;
    }

    node = lxb_dom_interface_node(element);

    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        lxb_style_resolver_invalidate(resolver, lxb_dom_interface_element(node));
    }

    child = node->first_child;

    while (child != NULL) {
        if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            lxb_style_resolver_invalidate_subtree(resolver,
                                        lxb_dom_interface_element(child));
        }

        child = child->next;
    }
}

static lxb_status_t
lxb_style_resolver_resolve_subtree(lxb_style_resolver_t *resolver,
                                   lxb_dom_node_t *node)
{
    lxb_dom_node_t *child;
    const lxb_style_computed_t *style;

    if (node == NULL) {
        return LXB_STATUS_OK;
    }

    if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        style = lxb_style_resolver_resolve_element(resolver,
                                        lxb_dom_interface_element(node));
        if (style == NULL) {
            return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
        }
    }

    child = node->first_child;

    while (child != NULL) {
        if (lxb_style_resolver_resolve_subtree(resolver, child) != LXB_STATUS_OK)
        {
            return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
        }

        child = child->next;
    }

    return LXB_STATUS_OK;
}

static lxb_status_t
lxb_style_resolver_initial_set(lxb_style_resolver_t *resolver)
{
    return lxb_style_computed_set_initial(&resolver->initial,
                                          resolver->ctx.initial_font_size,
                                          resolver->ctx.initial_line_height);
}
