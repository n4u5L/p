/*
 * Copyright (C) 2026 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/selectors/selector_filter.h"

#include "lexbor/core/hash.h"
#include "lexbor/core/lexbor.h"
#include "lexbor/core/str.h"
#include "lexbor/core/utils.h"
#include "lexbor/css/selectors/pseudo_const.h"
#include "lexbor/dom/interfaces/attr.h"
#include "lexbor/tag/tag.h"

#include <string.h>


#define LXB_SELECTORS_FILTER_BITS 12
#define LXB_SELECTORS_FILTER_SIZE ((size_t) 1 << LXB_SELECTORS_FILTER_BITS)
#define LXB_SELECTORS_FILTER_MASK (LXB_SELECTORS_FILTER_SIZE - 1)

#define LXB_SELECTORS_FILTER_STACK_INITIAL 20
#define LXB_SELECTORS_FILTER_FRAME_HASHES_INITIAL 8
#define LXB_SELECTORS_FILTER_COLLECTED_HASHES 16

#define LXB_SELECTORS_FILTER_TAG_NAME_SALT 13
#define LXB_SELECTORS_FILTER_ID_SALT 17
#define LXB_SELECTORS_FILTER_CLASS_SALT 19
#define LXB_SELECTORS_FILTER_ATTRIBUTE_SALT 23


typedef struct {
    unsigned values[LXB_SELECTORS_FILTER_COLLECTED_HASHES];
    size_t length;
}
lxb_selectors_filter_hash_bucket_t;

typedef struct {
    lxb_selectors_filter_hash_bucket_t ids;
    lxb_selectors_filter_hash_bucket_t classes;
    lxb_selectors_filter_hash_bucket_t tags;
    lxb_selectors_filter_hash_bucket_t attributes;
}
lxb_selectors_filter_collected_t;

typedef struct {
    lxb_dom_node_t *node;
    unsigned      *hashes;
    size_t        length;
    size_t        capacity;
}
lxb_selectors_filter_parent_t;

struct lxb_selectors_filter {
    uint32_t                      table[LXB_SELECTORS_FILTER_SIZE];
    lxb_selectors_filter_parent_t *stack;
    size_t                        stack_len;
    size_t                        stack_cap;
};


static lxb_status_t
lxb_selectors_filter_stack_grow(lxb_selectors_filter_t *filter);

static lxb_status_t
lxb_selectors_filter_parent_hash_append(lxb_selectors_filter_t *filter,
                                        lxb_selectors_filter_parent_t *parent,
                                        unsigned hash);

static lxb_status_t
lxb_selectors_filter_collect_element_identifier_hashes(
    lxb_selectors_filter_t *filter, lxb_selectors_filter_parent_t *parent,
    lxb_dom_element_t *element);

static unsigned
lxb_selectors_filter_hash(const lxb_char_t *data, size_t length,
                          unsigned salt);

static void
lxb_selectors_filter_collect_selector_hashes(
    lxb_selectors_filter_collected_t *collected,
    const lxb_css_selector_t *rightmost, bool include_rightmost);

static void
lxb_selectors_filter_collect_simple_selector_hash(
    lxb_selectors_filter_collected_t *collected,
    const lxb_css_selector_t *selector);

static void
lxb_selectors_filter_hash_bucket_append(
    lxb_selectors_filter_hash_bucket_t *bucket, unsigned hash);

static bool
lxb_selectors_filter_attr_is_excluded(const lxb_char_t *name, size_t length);


lxb_selectors_filter_t *
lxb_selectors_filter_create(void)
{
    return lexbor_calloc(1, sizeof(lxb_selectors_filter_t));
}

lxb_status_t
lxb_selectors_filter_init(lxb_selectors_filter_t *filter)
{
    if (filter == NULL) {
        return LXB_STATUS_ERROR_INCOMPLETE_OBJECT;
    }

    memset(filter->table, 0, sizeof(filter->table));
    filter->stack_len = 0;

    return LXB_STATUS_OK;
}

void
lxb_selectors_filter_clean(lxb_selectors_filter_t *filter)
{
    size_t i;

    if (filter == NULL) {
        return;
    }

    memset(filter->table, 0, sizeof(filter->table));

    for (i = 0; i < filter->stack_len; i++) {
        filter->stack[i].node = NULL;
        filter->stack[i].length = 0;
    }

    filter->stack_len = 0;
}

lxb_selectors_filter_t *
lxb_selectors_filter_destroy(lxb_selectors_filter_t *filter, bool self_destroy)
{
    size_t i;

    if (filter == NULL) {
        return NULL;
    }

    if (filter->stack != NULL) {
        for (i = 0; i < filter->stack_cap; i++) {
            if (filter->stack[i].hashes != NULL) {
                filter->stack[i].hashes = lexbor_free(filter->stack[i].hashes);
            }
        }

        filter->stack = lexbor_free(filter->stack);
    }

    filter->stack_len = 0;
    filter->stack_cap = 0;

    if (self_destroy) {
        return lexbor_free(filter);
    }

    return filter;
}

lxb_status_t
lxb_selectors_filter_push_parent(lxb_selectors_filter_t *filter,
                                 lxb_dom_element_t *parent)
{
    lxb_status_t status;
    lxb_selectors_filter_parent_t *frame;

    if (filter->stack_len == filter->stack_cap) {
        status = lxb_selectors_filter_stack_grow(filter);
        if (status != LXB_STATUS_OK) {
            return status;
        }
    }

    frame = &filter->stack[filter->stack_len];
    frame->node = lxb_dom_interface_node(parent);
    frame->length = 0;

    status = lxb_selectors_filter_collect_element_identifier_hashes(filter,
                                                                    frame,
                                                                    parent);
    if (status != LXB_STATUS_OK) {
        while (frame->length != 0) {
            frame->length--;
            if (filter->table[frame->hashes[frame->length]
                              & LXB_SELECTORS_FILTER_MASK] != 0)
            {
                filter->table[frame->hashes[frame->length]
                              & LXB_SELECTORS_FILTER_MASK]--;
            }
        }

        frame->node = NULL;
        return status;
    }

    filter->stack_len++;

    return LXB_STATUS_OK;
}

void
lxb_selectors_filter_pop_parent(lxb_selectors_filter_t *filter)
{
    size_t i;
    unsigned hash;
    lxb_selectors_filter_parent_t *frame;

    if (filter == NULL || filter->stack_len == 0) {
        return;
    }

    frame = &filter->stack[filter->stack_len - 1];

    for (i = 0; i < frame->length; i++) {
        hash = frame->hashes[i] & LXB_SELECTORS_FILTER_MASK;

        if (filter->table[hash] != 0) {
            filter->table[hash]--;
        }
    }

    frame->node = NULL;
    frame->length = 0;
    filter->stack_len--;

    if (filter->stack_len == 0) {
        memset(filter->table, 0, sizeof(filter->table));
    }
}

void
lxb_selectors_filter_pop_parents_until(lxb_selectors_filter_t *filter,
                                       lxb_dom_node_t *parent)
{
    if (filter == NULL) {
        return;
    }

    while (filter->stack_len != 0) {
        if (parent != NULL
            && filter->stack[filter->stack_len - 1].node == parent)
        {
            return;
        }

        lxb_selectors_filter_pop_parent(filter);
    }
}

bool
lxb_selectors_filter_parent_stack_is_consistent(
    const lxb_selectors_filter_t *filter, const lxb_dom_node_t *parent)
{
    if (filter == NULL) {
        return parent == NULL;
    }

    if (parent == NULL || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return filter->stack_len == 0;
    }

    return filter->stack_len != 0
           && filter->stack[filter->stack_len - 1].node == parent;
}

lxb_status_t
lxb_selectors_filter_initialize_parent_stack(lxb_selectors_filter_t *filter,
                                             lxb_dom_node_t *parent)
{
    lxb_status_t status;
    lxb_dom_node_t *node;
    lxb_dom_node_t **ancestors, **tmp;
    size_t length, capacity, i;

    if (filter == NULL) {
        return LXB_STATUS_ERROR_INCOMPLETE_OBJECT;
    }

    lxb_selectors_filter_clean(filter);

    ancestors = NULL;
    length = 0;
    capacity = 0;

    for (node = parent; node != NULL; node = node->parent) {
        if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
            continue;
        }

        if (length == capacity) {
            capacity = capacity == 0 ? LXB_SELECTORS_FILTER_STACK_INITIAL
                                     : capacity * 2;

            tmp = lexbor_realloc(ancestors,
                                 capacity * sizeof(lxb_dom_node_t *));
            if (tmp == NULL) {
                if (ancestors != NULL) {
                    ancestors = lexbor_free(ancestors);
                }

                return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
            }

            ancestors = tmp;
        }

        ancestors[length++] = node;
    }

    for (i = length; i != 0; i--) {
        status = lxb_selectors_filter_push_parent(filter,
                       lxb_dom_interface_element(ancestors[i - 1]));
        if (status != LXB_STATUS_OK) {
            if (ancestors != NULL) {
                ancestors = lexbor_free(ancestors);
            }

            lxb_selectors_filter_clean(filter);
            return status;
        }
    }

    if (ancestors != NULL) {
        ancestors = lexbor_free(ancestors);
    }

    return LXB_STATUS_OK;
}

bool
lxb_selectors_filter_fast_reject(const lxb_selectors_filter_t *filter,
                                 const lxb_selectors_filter_hashes_t *hashes)
{
    size_t i;
    unsigned hash;

    if (filter == NULL || hashes == NULL) {
        return false;
    }

    for (i = 0; i < LXB_SELECTORS_FILTER_HASHES; i++) {
        hash = hashes->hashes[i];

        if (hash == 0) {
            return false;
        }

        if (filter->table[hash & LXB_SELECTORS_FILTER_MASK] == 0) {
            return true;
        }
    }

    return false;
}

lxb_selectors_filter_hashes_t
lxb_selectors_filter_collect_hashes(const lxb_css_selector_t *selector)
{
    size_t i, j;
    unsigned hash;
    lxb_selectors_filter_collected_t collected;
    lxb_selectors_filter_hashes_t hashes;

    memset(&collected, 0, sizeof(collected));
    memset(&hashes, 0, sizeof(hashes));

    lxb_selectors_filter_collect_selector_hashes(&collected, selector, false);

#define LXB_SELECTORS_FILTER_COPY_BUCKET(bucket)                              \
    do {                                                                       \
        for (i = 0; i < (bucket).length; i++) {                                \
            hash = (bucket).values[i];                                         \
            for (j = 0; j < LXB_SELECTORS_FILTER_HASHES; j++) {                \
                if (hashes.hashes[j] == hash) {                                \
                    break;                                                     \
                }                                                              \
                if (hashes.hashes[j] == 0) {                                   \
                    hashes.hashes[j] = hash;                                   \
                    break;                                                     \
                }                                                              \
            }                                                                  \
            if (hashes.hashes[LXB_SELECTORS_FILTER_HASHES - 1] != 0) {         \
                return hashes;                                                 \
            }                                                                  \
        }                                                                      \
    }                                                                          \
    while (0)

    LXB_SELECTORS_FILTER_COPY_BUCKET(collected.ids);
    LXB_SELECTORS_FILTER_COPY_BUCKET(collected.attributes);
    LXB_SELECTORS_FILTER_COPY_BUCKET(collected.classes);
    LXB_SELECTORS_FILTER_COPY_BUCKET(collected.tags);

#undef LXB_SELECTORS_FILTER_COPY_BUCKET

    return hashes;
}

static lxb_status_t
lxb_selectors_filter_stack_grow(lxb_selectors_filter_t *filter)
{
    size_t old_cap, new_cap;
    lxb_selectors_filter_parent_t *stack;

    old_cap = filter->stack_cap;
    new_cap = old_cap == 0 ? LXB_SELECTORS_FILTER_STACK_INITIAL : old_cap * 2;

    stack = lexbor_realloc(filter->stack,
                           new_cap * sizeof(lxb_selectors_filter_parent_t));
    if (stack == NULL) {
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    memset(&stack[old_cap], 0,
           (new_cap - old_cap) * sizeof(lxb_selectors_filter_parent_t));

    filter->stack = stack;
    filter->stack_cap = new_cap;

    return LXB_STATUS_OK;
}

static lxb_status_t
lxb_selectors_filter_parent_hash_append(lxb_selectors_filter_t *filter,
                                        lxb_selectors_filter_parent_t *parent,
                                        unsigned hash)
{
    size_t new_cap;
    unsigned *hashes;

    if (hash == 0) {
        return LXB_STATUS_OK;
    }

    if (parent->length == parent->capacity) {
        new_cap = parent->capacity == 0
                  ? LXB_SELECTORS_FILTER_FRAME_HASHES_INITIAL
                  : parent->capacity * 2;

        hashes = lexbor_realloc(parent->hashes, new_cap * sizeof(unsigned));
        if (hashes == NULL) {
            return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
        }

        parent->hashes = hashes;
        parent->capacity = new_cap;
    }

    parent->hashes[parent->length++] = hash;

    hash = hash & LXB_SELECTORS_FILTER_MASK;
    if (filter->table[hash] != UINT32_MAX) {
        filter->table[hash]++;
    }

    return LXB_STATUS_OK;
}

static lxb_status_t
lxb_selectors_filter_collect_element_identifier_hashes(
    lxb_selectors_filter_t *filter, lxb_selectors_filter_parent_t *parent,
    lxb_dom_element_t *element)
{
    lxb_status_t status;
    lxb_dom_attr_t *attr;
    const lxb_char_t *data, *end, *pos;
    size_t length;

    data = lxb_tag_name_by_id(lxb_dom_element_tag_id(element), &length);
    status = lxb_selectors_filter_parent_hash_append(filter, parent,
             lxb_selectors_filter_hash(data, length,
                                       LXB_SELECTORS_FILTER_TAG_NAME_SALT));
    if (status != LXB_STATUS_OK) {
        return status;
    }

    data = lxb_dom_element_id(element, &length);
    status = lxb_selectors_filter_parent_hash_append(filter, parent,
             lxb_selectors_filter_hash(data, length,
                                       LXB_SELECTORS_FILTER_ID_SALT));
    if (status != LXB_STATUS_OK) {
        return status;
    }

    data = lxb_dom_element_class(element, &length);
    if (data != NULL && length != 0) {
        end = data + length;
        pos = data;

        while (data < end) {
            if (lexbor_utils_whitespace(*data, ==, ||)) {
                status = lxb_selectors_filter_parent_hash_append(filter,
                         parent, lxb_selectors_filter_hash(pos, data - pos,
                                             LXB_SELECTORS_FILTER_CLASS_SALT));
                if (status != LXB_STATUS_OK) {
                    return status;
                }

                pos = data + 1;
            }

            data++;
        }

        status = lxb_selectors_filter_parent_hash_append(filter, parent,
                 lxb_selectors_filter_hash(pos, end - pos,
                                           LXB_SELECTORS_FILTER_CLASS_SALT));
        if (status != LXB_STATUS_OK) {
            return status;
        }
    }

    attr = lxb_dom_element_first_attribute(element);
    while (attr != NULL) {
        switch (attr->node.local_name) {
            case LXB_DOM_ATTR_ID:
            case LXB_DOM_ATTR_CLASS:
            case LXB_DOM_ATTR_STYLE:
                attr = lxb_dom_element_next_attribute(attr);
                continue;

            default:
                break;
        }

        data = lxb_dom_attr_local_name(attr, &length);
        status = lxb_selectors_filter_parent_hash_append(filter, parent,
                 lxb_selectors_filter_hash(data, length,
                                  LXB_SELECTORS_FILTER_ATTRIBUTE_SALT));
        if (status != LXB_STATUS_OK) {
            return status;
        }

        attr = lxb_dom_element_next_attribute(attr);
    }

    return LXB_STATUS_OK;
}

static unsigned
lxb_selectors_filter_hash(const lxb_char_t *data, size_t length,
                          unsigned salt)
{
    unsigned hash;

    if (data == NULL || length == 0) {
        return 0;
    }

    hash = lexbor_hash_make_id_lower(data, length) * salt;

    return hash == 0 ? salt : hash;
}

static void
lxb_selectors_filter_collect_selector_hashes(
    lxb_selectors_filter_collected_t *collected,
    const lxb_css_selector_t *rightmost, bool include_rightmost)
{
    bool skip;
    const lxb_css_selector_t *selector;
    lxb_css_selector_combinator_t combinator;

    if (rightmost == NULL) {
        return;
    }

    if (include_rightmost) {
        selector = rightmost;
        combinator = LXB_CSS_SELECTOR_COMBINATOR_CLOSE;
        skip = false;
    }
    else {
        selector = rightmost->prev;
        combinator = rightmost->combinator;
        skip = true;
    }

    while (selector != NULL) {
        switch (combinator) {
            case LXB_CSS_SELECTOR_COMBINATOR_CLOSE:
                if (!skip) {
                    lxb_selectors_filter_collect_simple_selector_hash(collected,
                                                                      selector);
                }
                break;

            case LXB_CSS_SELECTOR_COMBINATOR_DESCENDANT:
            case LXB_CSS_SELECTOR_COMBINATOR_CHILD:
                skip = false;
                lxb_selectors_filter_collect_simple_selector_hash(collected,
                                                                  selector);
                break;

            case LXB_CSS_SELECTOR_COMBINATOR_SIBLING:
            case LXB_CSS_SELECTOR_COMBINATOR_FOLLOWING:
            case LXB_CSS_SELECTOR_COMBINATOR_CELL:
            default:
                skip = true;
                break;
        }

        combinator = selector->combinator;
        selector = selector->prev;
    }
}

static void
lxb_selectors_filter_collect_simple_selector_hash(
    lxb_selectors_filter_collected_t *collected,
    const lxb_css_selector_t *selector)
{
    lxb_css_selector_list_t *list;
    const lxb_css_selector_pseudo_t *pseudo;

    switch (selector->type) {
        case LXB_CSS_SELECTOR_TYPE_ID:
            lxb_selectors_filter_hash_bucket_append(&collected->ids,
                lxb_selectors_filter_hash(selector->name.data,
                                          selector->name.length,
                                          LXB_SELECTORS_FILTER_ID_SALT));
            break;

        case LXB_CSS_SELECTOR_TYPE_CLASS:
            lxb_selectors_filter_hash_bucket_append(&collected->classes,
                lxb_selectors_filter_hash(selector->name.data,
                                          selector->name.length,
                                          LXB_SELECTORS_FILTER_CLASS_SALT));
            break;

        case LXB_CSS_SELECTOR_TYPE_ELEMENT:
            if (selector->name.length == 1 && selector->name.data != NULL
                && selector->name.data[0] == '*')
            {
                break;
            }

            lxb_selectors_filter_hash_bucket_append(&collected->tags,
                lxb_selectors_filter_hash(selector->name.data,
                                          selector->name.length,
                                          LXB_SELECTORS_FILTER_TAG_NAME_SALT));
            break;

        case LXB_CSS_SELECTOR_TYPE_ATTRIBUTE:
            if (lxb_selectors_filter_attr_is_excluded(selector->name.data,
                                                      selector->name.length))
            {
                break;
            }

            lxb_selectors_filter_hash_bucket_append(&collected->attributes,
                lxb_selectors_filter_hash(selector->name.data,
                                  selector->name.length,
                                  LXB_SELECTORS_FILTER_ATTRIBUTE_SALT));
            break;

        case LXB_CSS_SELECTOR_TYPE_PSEUDO_CLASS_FUNCTION:
            pseudo = &selector->u.pseudo;
            if (pseudo->type != LXB_CSS_SELECTOR_PSEUDO_CLASS_FUNCTION_IS
                && pseudo->type != LXB_CSS_SELECTOR_PSEUDO_CLASS_FUNCTION_WHERE)
            {
                break;
            }

            list = (lxb_css_selector_list_t *) pseudo->data;
            if (list != NULL && list->next == NULL) {
                lxb_selectors_filter_collect_selector_hashes(collected,
                                                             list->last, true);
            }

            break;

        default:
            break;
    }
}

static void
lxb_selectors_filter_hash_bucket_append(
    lxb_selectors_filter_hash_bucket_t *bucket, unsigned hash)
{
    size_t i;

    if (hash == 0) {
        return;
    }

    for (i = 0; i < bucket->length; i++) {
        if (bucket->values[i] == hash) {
            return;
        }
    }

    if (bucket->length == LXB_SELECTORS_FILTER_COLLECTED_HASHES) {
        return;
    }

    bucket->values[bucket->length++] = hash;
}

static bool
lxb_selectors_filter_attr_is_excluded(const lxb_char_t *name, size_t length)
{
    if (name == NULL) {
        return false;
    }

    switch (length) {
        case 2:
            return lexbor_str_data_ncasecmp(name, (const lxb_char_t *) "id",
                                            length);

        case 5:
            return lexbor_str_data_ncasecmp(name, (const lxb_char_t *) "class",
                                            length)
                   || lexbor_str_data_ncasecmp(name,
                                               (const lxb_char_t *) "style",
                                               length);

        default:
            return false;
    }
}
