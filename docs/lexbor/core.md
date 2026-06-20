# core 模块
core 模块是 lexbor 的基础设施。

## Array
### `lexbor_array_t`
provided by `lexbor/core/array.h`
```c
LXB_API lexbor_array_t *
lexbor_array_create(void);

LXB_API lxb_status_t
lexbor_array_init(lexbor_array_t *array, size_t size);

LXB_API lxb_status_t
lexbor_array_push(lexbor_array_t *array, void *value);

LXB_API void *
lexbor_array_pop(lexbor_array_t *array);

LXB_API void
lexbor_array_clean(lexbor_array_t *array);

LXB_API lexbor_array_t *
lexbor_array_destroy(lexbor_array_t *array, bool self_destroy);

LXB_API void **
lexbor_array_expand(lexbor_array_t *array, size_t up_to);

LXB_API lxb_status_t
lexbor_array_insert(lexbor_array_t *array, size_t idx, void *value);

LXB_API lxb_status_t
lexbor_array_set(lexbor_array_t *array, size_t idx, void *value);

LXB_API void
lexbor_array_delete(lexbor_array_t *array, size_t begin, size_t length);
```

## Memory Pool
### `lexbor_mem_t`
provided by `lexbor/core/mem.h`  
`lexbor_mem_t` 是一个节点为 `lexbor_mem_chunk_t` 的双向链表.
```c
// 创建一个空 `lexbor_mem_t` 对象.
LXB_API lexbor_mem_t *
lexbor_mem_create(void);

// 初始化1个 `lexbor_mem_t` 对象. 会分配1个`min_chunk_size` 大小的 chunk.
LXB_API lxb_status_t
lexbor_mem_init(lexbor_mem_t *mem, size_t min_chunk_size);

// 当前 chunk 的剩余容量不足以分配1个对象时，会分配1个能够容纳对象的新的 chunk.
LXB_API void *
lexbor_mem_alloc(lexbor_mem_t *mem, size_t length);

// 释放所有 chunk.
LXB_API lexbor_mem_t *
lexbor_mem_destroy(lexbor_mem_t *mem, bool destroy_self);

// 保留第1个 chunk, 释放其他 chunk.
LXB_API void
lexbor_mem_clean(lexbor_mem_t *mem);

LXB_API void *
lexbor_mem_calloc(lexbor_mem_t *mem, size_t length);
```  

### `lexbor_dobject_t`

### `lexbor_mraw_t`
provided by `lexbor/core/mraw.h`
```c
LXB_API lexbor_mraw_t *
lexbor_mraw_create(void);

LXB_API lxb_status_t
lexbor_mraw_init(lexbor_mraw_t *mraw, size_t chunk_size);

LXB_API void
lexbor_mraw_clean(lexbor_mraw_t *mraw);

LXB_API lexbor_mraw_t *
lexbor_mraw_destroy(lexbor_mraw_t *mraw, bool destroy_self);

LXB_API void *
lexbor_mraw_alloc(lexbor_mraw_t *mraw, size_t size);

LXB_API void *
lexbor_mraw_calloc(lexbor_mraw_t *mraw, size_t size);

LXB_API void *
lexbor_mraw_realloc(lexbor_mraw_t *mraw, void *data, size_t new_size);

LXB_API void *
lexbor_mraw_free(lexbor_mraw_t *mraw, void *data);
```

## AVL Tree

## String

