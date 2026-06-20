/*
 * Copyright (C) 2025-2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/style/style.h"
#include "lexbor/html/interfaces/document.h"


static lexbor_action_t
lxb_style_computed_destroy_cb(lxb_dom_node_t *node, void *ctx);

static void
lxb_style_computed_destroy_all(lxb_html_document_t *doc);


static lexbor_action_t
lxb_style_computed_destroy_cb(lxb_dom_node_t *node, void *ctx)
{
    lxb_dom_element_t *element;

    (void) ctx;

    if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return LEXBOR_ACTION_OK;
    }

    element = lxb_dom_interface_element(node);
    if (element->computed_style != NULL) {
        lxb_style_computed_unref(element->computed_style);
        element->computed_style = NULL;
    }

    return LEXBOR_ACTION_OK;
}

static void
lxb_style_computed_destroy_all(lxb_html_document_t *doc)
{
    lxb_dom_node_t *root;

    if (doc == NULL) {
        return;
    }

    root = lxb_dom_interface_node(doc);

    if (root->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        (void) lxb_style_computed_destroy_cb(root, NULL);
    }

    lxb_dom_node_simple_walk(root, lxb_style_computed_destroy_cb, NULL);
}


uintptr_t
lxb_style_id_by_name(const lxb_dom_document_t *doc,
                     const lxb_char_t *name, size_t size)
{
    const lxb_css_entry_data_t *data;

    data = lxb_css_property_by_name(name, size);

    if (data == NULL) {
        return lxb_dom_document_css_customs_find_id(doc, name, size);
    }

    return data->unique;
}

lxb_status_t
lxb_style_init(lxb_html_document_t *doc)
{
    if (doc == NULL) {
        return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    doc->done = lxb_html_document_done_cb;

    lxb_html_document_style_mutation_init(doc);

    return lxb_dom_document_css_init(lxb_dom_interface_document(doc), false);
}

void
lxb_style_destroy(lxb_html_document_t *doc)
{
    if (doc == NULL) {
        return;
    }

    lxb_style_computed_destroy_all(doc);

    doc->done = lxb_html_document_done_cb;

    lxb_html_document_style_mutation_erase(doc);

    lxb_dom_document_css_destroy(lxb_dom_interface_document(doc));
}
