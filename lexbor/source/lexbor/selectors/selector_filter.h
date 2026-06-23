/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#ifndef LEXBOR_SELECTORS_SELECTOR_FILTER_H
#define LEXBOR_SELECTORS_SELECTOR_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lexbor/selectors/base.h"
#include "lexbor/css/selectors/selector.h"
#include "lexbor/dom/interfaces/element.h"


#define LXB_SELECTORS_FILTER_HASHES 4

typedef struct {
    unsigned hashes[LXB_SELECTORS_FILTER_HASHES];
}
lxb_selectors_filter_hashes_t;

typedef struct lxb_selectors_filter lxb_selectors_filter_t;


LXB_API lxb_selectors_filter_t *
lxb_selectors_filter_create(void);

LXB_API lxb_status_t
lxb_selectors_filter_init(lxb_selectors_filter_t *filter);

LXB_API void
lxb_selectors_filter_clean(lxb_selectors_filter_t *filter);

LXB_API lxb_selectors_filter_t *
lxb_selectors_filter_destroy(lxb_selectors_filter_t *filter, bool self_destroy);

LXB_API lxb_status_t
lxb_selectors_filter_push_parent(lxb_selectors_filter_t *filter,
                                 lxb_dom_element_t *parent);

LXB_API void
lxb_selectors_filter_pop_parent(lxb_selectors_filter_t *filter);

LXB_API void
lxb_selectors_filter_pop_parents_until(lxb_selectors_filter_t *filter,
                                       lxb_dom_node_t *parent);

LXB_API bool
lxb_selectors_filter_parent_stack_is_consistent(
    const lxb_selectors_filter_t *filter, const lxb_dom_node_t *parent);

LXB_API lxb_status_t
lxb_selectors_filter_initialize_parent_stack(lxb_selectors_filter_t *filter,
                                             lxb_dom_node_t *parent);

LXB_API bool
lxb_selectors_filter_fast_reject(const lxb_selectors_filter_t *filter,
                                 const lxb_selectors_filter_hashes_t *hashes);

LXB_API lxb_selectors_filter_hashes_t
lxb_selectors_filter_collect_hashes(const lxb_css_selector_t *selector);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_SELECTORS_SELECTOR_FILTER_H */
