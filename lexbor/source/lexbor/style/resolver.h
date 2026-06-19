/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#ifndef LEXBOR_STYLE_RESOLVER_H
#define LEXBOR_STYLE_RESOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lexbor/style/computed.h"
#include "lexbor/html/interfaces/document.h"
#include "lexbor/dom/interfaces/element.h"


typedef struct lxb_style_resolver lxb_style_resolver_t;

typedef struct {
    double viewport_width;
    double viewport_height;
    double initial_font_size;
    double initial_line_height;
}
lxb_style_resolve_context_t;

typedef struct {
    lxb_style_resolver_t              *resolver;
    lxb_dom_element_t                 *element;
    const lxb_style_computed_t        *parent;
    lxb_style_computed_t              *style;
    const lxb_style_resolve_context_t *ctx;
}
lxb_style_compute_ctx_t;


LXB_API lxb_style_resolver_t *
lxb_style_resolver_create(void);

LXB_API lxb_status_t
lxb_style_resolver_init(lxb_style_resolver_t *resolver);

LXB_API void
lxb_style_resolver_clean(lxb_style_resolver_t *resolver);

LXB_API lxb_style_resolver_t *
lxb_style_resolver_destroy(lxb_style_resolver_t *resolver, bool self_destroy);

LXB_API void
lxb_style_resolver_context_set(lxb_style_resolver_t *resolver,
                               const lxb_style_resolve_context_t *ctx);

LXB_API lxb_status_t
lxb_style_resolver_resolve_document(lxb_style_resolver_t *resolver,
                                    lxb_html_document_t *document);

LXB_API const lxb_style_computed_t *
lxb_style_resolver_resolve_element(lxb_style_resolver_t *resolver,
                                   lxb_dom_element_t *element);

LXB_API const lxb_style_computed_t *
lxb_style_resolver_style_by_element(lxb_style_resolver_t *resolver,
                                    const lxb_dom_element_t *element);

LXB_API void
lxb_style_resolver_invalidate(lxb_style_resolver_t *resolver,
                              lxb_dom_element_t *element);

LXB_API void
lxb_style_resolver_invalidate_subtree(lxb_style_resolver_t *resolver,
                                      lxb_dom_element_t *element);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_STYLE_RESOLVER_H */

